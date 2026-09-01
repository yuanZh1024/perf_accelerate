package main

func main() {

	go startMountd()

	startNFSServer()
}
