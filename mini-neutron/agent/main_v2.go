package main

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os/exec"
	"time"
)

type Port struct {
	ID        string `json:"id"`
	NetworkID string `json:"network_id"`
	MAC       string `json:"mac"`
	FixedIP   string `json:"fixed_ip"`
	Host      string `json:"host"`
}

func main() {
	fmt.Println("Mini OVS Agent starting...")

	for {
		syncPorts()

		time.Sleep(3 * time.Second)
	}
}

func syncPorts() {
	ports, err := fetchPorts()
	if err != nil {
		fmt.Println("[agent] failed to fetch ports:", err)
		return
	}

	for _, port := range ports {

		if port.Host != "compute-1" {
			continue
		}

		fmt.Printf(
			"[agent] port=%s mac=%s ip=%s host=%s\n",
			port.ID,
			port.MAC,
			port.FixedIP,
			port.Host,
		)

		realizePort(port)
	}
}

func fetchPorts() ([]Port, error) {

	resp, err := http.Get("http://localhost:9696/ports")
	if err != nil {
		return nil, err
	}

	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}

	var ports []Port

	if err := json.Unmarshal(body, &ports); err != nil {
		return nil, err
	}

	return ports, nil
}

func realizePort(port Port) {

	ns := "vm-" + port.ID

	ovsIf := "ovs-" + port.ID
	vmIf := "eth0"

	/*
	 * 1. 创建 namespace
	 */
	if !namespaceExists(ns) {

		fmt.Printf(
			"[agent] create namespace %s\n",
			ns,
		)

		run(
			"ip",
			"netns",
			"add",
			ns,
		)
	}

	/*
	 * 2. 检查 namespace 中是否已经有 eth0
	 */
	if interfaceExists(ns, vmIf) {
		fmt.Printf(
			"[agent] port %s already realized\n",
			port.ID,
		)

		return
	}

	/*
	 * 3. 创建 veth pair
	 */

	fmt.Printf(
		"[agent] creating veth pair: %s <-> %s\n",
		ovsIf,
		vmIf,
	)

	run(
		"ip",
		"link",
		"add",
		ovsIf,
		"type",
		"veth",
		"peer",
		"name",
		vmIf,
	)

	/*
	 * 4. 把 VM 这一端移动到 namespace
	 */

	run(
		"ip",
		"link",
		"set",
		vmIf,
		"netns",
		ns,
	)

	/*
	 * 5. 把 ovs 这一端接入 OVS
	 */

	fmt.Printf(
		"[agent] adding %s to br-int\n",
		ovsIf,
	)

	run(
		"ovs-vsctl",
		"--may-exist",
		"add-port",
		"br-int",
		ovsIf,
	)

	/*
	 * 6. 配置 VM 端
	 */

	run(
		"ip",
		"netns",
		"exec",
		ns,
		"ip",
		"link",
		"set",
		"lo",
		"up",
	)

	run(
		"ip",
		"netns",
		"exec",
		ns,
		"ip",
		"link",
		"set",
		"eth0",
		"address",
		port.MAC,
	)

	run(
		"ip",
		"netns",
		"exec",
		ns,
		"ip",
		"addr",
		"add",
		port.FixedIP+"/24",
		"dev",
		"eth0",
	)

	run(
		"ip",
		"netns",
		"exec",
		ns,
		"ip",
		"link",
		"set",
		"eth0",
		"up",
	)

	fmt.Printf(
		"[agent] port %s successfully realized\n",
		port.ID,
	)
}

func namespaceExists(ns string) bool {

	cmd := exec.Command(
		"ip",
		"netns",
		"list",
	)

	output, _ := cmd.Output()

	return contains(string(output), ns)
}

func interfaceExists(ns string, iface string) bool {

	cmd := exec.Command(
		"ip",
		"netns",
		"exec",
		ns,
		"ip",
		"link",
		"show",
		iface,
	)

	return cmd.Run() == nil
}

func run(name string, args ...string) {

	fmt.Printf(
		"[exec] %s %v\n",
		name,
		args,
	)

	cmd := exec.Command(name, args...)

	output, err := cmd.CombinedOutput()

	if err != nil {

		fmt.Printf(
			"[exec] ERROR: %v output=%s\n",
			err,
			string(output),
		)
	}
}

func contains(s string, target string) bool {

	for _, line := range splitLines(s) {

		if len(line) >= len(target) &&
			line[:len(target)] == target {

			return true
		}
	}

	return false
}

func splitLines(s string) []string {

	var result []string
	current := ""

	for _, c := range s {

		if c == '\n' {

			if current != "" {
				result = append(result, current)
			}

			current = ""

		} else {
			current += string(c)
		}
	}

	if current != "" {
		result = append(result, current)
	}

	return result
}
