package main

import (
	"encoding/json"
	"fmt"
	"net"
	"net/http"
	"sync"
)

type Network struct {
	ID   string `json:"id"`
	Name string `json:"name"`
}

type Subnet struct {
	ID        string `json:"id"`
	NetworkID string `json:"network_id"`
	CIDR      string `json:"cidr"`
}

type Port struct {
	ID        string `json:"id"`
	NetworkID string `json:"network_id"`
	MAC       string `json:"mac"`
	FixedIP   string `json:"fixed_ip"`
	Host      string `json:"host"`
}

type MiniNeutron struct {
	mu sync.Mutex

	networks []Network
	subnets  []Subnet
	ports    []Port

	nextNetwork int
	nextSubnet  int
	nextPort    int
	nextIP      int
}

func main() {
	n := &MiniNeutron{}

	http.HandleFunc("/networks", func(w http.ResponseWriter, r *http.Request) {
		switch r.Method {
		case http.MethodPost:
			n.createNetwork(w, r)
		case http.MethodGet:
			n.listNetworks(w, r)
		default:
			http.Error(w, "method not allowed", 405)
		}
	})

	http.HandleFunc("/subnets", func(w http.ResponseWriter, r *http.Request) {
		if r.Method == http.MethodPost {
			n.createSubnet(w, r)
			return
		}

		http.Error(w, "method not allowed", 405)
	})

	http.HandleFunc("/ports", func(w http.ResponseWriter, r *http.Request) {
		switch r.Method {
		case http.MethodPost:
			n.createPort(w, r)
		case http.MethodGet:
			n.listPorts(w, r)
		default:
			http.Error(w, "method not allowed", 405)
		}
	})

	fmt.Println("Mini-Neutron Server listening on :9696")

	err := http.ListenAndServe(":9696", nil)
	if err != nil {
		panic(err)
	}
}

func (n *MiniNeutron) createNetwork(w http.ResponseWriter, r *http.Request) {
	var req struct {
		Name string `json:"name"`
	}

	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		http.Error(w, err.Error(), 400)
		return
	}

	n.mu.Lock()
	defer n.mu.Unlock()

	n.nextNetwork++

	network := Network{
		ID:   fmt.Sprintf("net-%03d", n.nextNetwork),
		Name: req.Name,
	}

	n.networks = append(n.networks, network)

	writeJSON(w, network)
}

func (n *MiniNeutron) listNetworks(w http.ResponseWriter, r *http.Request) {
	n.mu.Lock()
	defer n.mu.Unlock()

	writeJSON(w, n.networks)
}

func (n *MiniNeutron) createSubnet(w http.ResponseWriter, r *http.Request) {
	var req struct {
		NetworkID string `json:"network_id"`
		CIDR      string `json:"cidr"`
	}

	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		http.Error(w, err.Error(), 400)
		return
	}

	if _, _, err := net.ParseCIDR(req.CIDR); err != nil {
		http.Error(w, "invalid CIDR", 400)
		return
	}

	n.mu.Lock()
	defer n.mu.Unlock()

	n.nextSubnet++

	subnet := Subnet{
		ID:        fmt.Sprintf("subnet-%03d", n.nextSubnet),
		NetworkID: req.NetworkID,
		CIDR:      req.CIDR,
	}

	n.subnets = append(n.subnets, subnet)

	writeJSON(w, subnet)
}

func (n *MiniNeutron) createPort(w http.ResponseWriter, r *http.Request) {
	var req struct {
		NetworkID string `json:"network_id"`
		Host      string `json:"host"`
	}

	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		http.Error(w, err.Error(), 400)
		return
	}

	n.mu.Lock()
	defer n.mu.Unlock()

	n.nextPort++
	n.nextIP++

	port := Port{
		ID: fmt.Sprintf("port-%03d", n.nextPort),

		NetworkID: req.NetworkID,

		MAC: fmt.Sprintf(
			"fa:16:3e:00:00:%02x",
			n.nextPort,
		),

		FixedIP: fmt.Sprintf(
			"10.0.1.%d",
			9+n.nextIP,
		),

		Host: req.Host,
	}

	n.ports = append(n.ports, port)

	writeJSON(w, port)
}

func (n *MiniNeutron) listPorts(w http.ResponseWriter, r *http.Request) {
	n.mu.Lock()
	defer n.mu.Unlock()

	writeJSON(w, n.ports)
}

func writeJSON(w http.ResponseWriter, value interface{}) {
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(value)
}
