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
	// 1. 从 Neutron Server 获取期望状态
	resp, err := fetchPorts()
	if err != nil {
		fmt.Println("failed to fetch ports:", err)
		return
	}

	// 2. 只处理属于本机 compute-1 的 Port
	for _, port := range resp {

		if port.Host != "compute-1" {
			continue
		}

		fmt.Printf(
			"[agent] found port: id=%s mac=%s ip=%s\n",
			port.ID,
			port.MAC,
			port.FixedIP,
		)

		// 3. 把逻辑 Port 落地到 OVS
		realizePort(port)
	}
}

func fetchPorts() ([]Port, error) {
	resp, err := httpGet("http://localhost:9696/ports")
	if err != nil {
		return nil, err
	}

	var ports []Port

	err = json.Unmarshal(resp, &ports)
	if err != nil {
		return nil, err
	}

	return ports, nil
}

func httpGet(url string) ([]byte, error) {

	resp, err := http.Get(url)
	if err != nil {
		return nil, err
	}

	defer resp.Body.Close()

	return io.ReadAll(resp.Body)
}

func realizePort(port Port) {

	ovsPortName := "tap-" + port.ID

	// 检查 OVS Port 是否已经存在
	cmd := exec.Command(
		"ovs-vsctl",
		"port-to-br",
		ovsPortName,
	)

	output, err := cmd.CombinedOutput()

	if err == nil {
		// 已经存在
		fmt.Printf(
			"[agent] port %s already realized on %s\n",
			port.ID,
			string(output),
		)

		return
	}

	fmt.Printf(
		"[agent] realizing port %s\n",
		port.ID,
	)

	// 创建一个 OVS internal port
	cmd = exec.Command(
		"ovs-vsctl",
		"--may-exist",
		"add-port",
		"br-int",
		ovsPortName,
	)

	output, err = cmd.CombinedOutput()

	if err != nil {
		fmt.Printf(
			"[agent] failed to add port: %s\n",
			string(output),
		)

		return
	}

	fmt.Printf(
		"[agent] OVS port created: %s\n",
		ovsPortName,
	)
}
