package main

import (
	"log"
	"net"
	"os"

	"github.com/willscott/go-nfs"
	"github.com/willscott/go-nfs/handlers"
	"github.com/willscott/go-nfs/mount"
)

func main() {
	root := "./export"

	os.MkdirAll(root, 0755)

	// NFS 文件系统
	handler := handlers.NewPanicHandler(
		handlers.FileSystem(root),
	)

	// NFS 服务
	nfsServer := &nfs.Server{
		Handler: handler,
	}

	// NFSv3
	nfsListener, err := net.Listen("tcp", ":2049")
	if err != nil {
		log.Fatal(err)
	}

	// MOUNT protocol
	mountListener, err := net.Listen("tcp", ":20048")
	if err != nil {
		log.Fatal(err)
	}

	go func() {
		log.Println("NFS server listening on :2049")

		if err := nfsServer.Serve(nfsListener); err != nil {
			log.Fatal(err)
		}
	}()

	log.Println("Mount server listening on :20048")

	mountServer := mount.NewServer(
		"/mount",
		handler,
	)

	if err := mountServer.Serve(mountListener); err != nil {
		log.Fatal(err)
	}
}
