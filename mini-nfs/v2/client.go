package main

/*
把 JSON 换成我们自己设计的二进制协议，理解“网络协议本质上就是一段字节”。
应用层必须自己定义：

┌──────────┬──────────┐
│ LENGTH   │ PAYLOAD  │
└──────────┴──────────┘

这就是所谓的 framing。

RPC、HTTP/2、各种二进制协议，本质上都会解决这个问题。

为了快速实验，这次把协议简化成：

Request:

┌──────┬──────┬──────────────┐
│ OP   │ LEN  │ PAYLOAD      │
│ 1B   │ 4B   │ LEN bytes    │
└──────┴──────┴──────────────┘

Response
┬──────┬──────────────┐
│ LEN  │ PAYLOAD      │
│ 4B   │ LEN bytes    │
└──────┴──────┴───────

*/
import (
	"encoding/binary"
	"fmt"
	"io"
	"net"
)

func main() {

	conn, err := net.Dial("tcp", "127.0.0.1:9000")

	if err != nil {
		panic(err)
	}

	defer conn.Close()

	// WRITE
	send(
		conn,
		3,
		"hello.txt|0|hello from binary NFS",
	)

	fmt.Println("WRITE:", receive(conn))

	// READ
	send(
		conn,
		2,
		"hello.txt|0|1024",
	)

	fmt.Println("READ:", receive(conn))
}

func send(conn net.Conn, op byte, payload string) {

	data := []byte(payload)

	// OP
	conn.Write([]byte{op})

	// LENGTH
	var lenBuf [4]byte

	/*
				// 把payload的长度写入lenBuf
		binary.BigEndian.PutUint32(lenBuf[:], uint32(len(payload)))
		// 反向读回来验证
		readBack := binary.BigEndian.Uint32(lenBuf[:])
	*/

	binary.BigEndian.PutUint32(
		lenBuf[:],
		uint32(len(data)),
	)

	conn.Write(lenBuf[:])

	// PAYLOAD
	conn.Write(data)
}

func receive(conn net.Conn) string {

	var lenBuf [4]byte

	io.ReadFull(conn, lenBuf[:])

	length := binary.BigEndian.Uint32(lenBuf[:])

	data := make([]byte, length)

	io.ReadFull(conn, data)

	return string(data)
}
