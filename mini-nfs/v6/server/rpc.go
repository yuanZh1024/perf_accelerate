package main

import (
	"encoding/binary"
	"fmt"
	"io"
	"net"
)

type RPCRequest struct {
	XID       uint32
	Program   uint32
	Version   uint32
	Procedure uint32
	Args      []byte
}

func readRPCRequest(conn net.Conn) (RPCRequest, error) {

	header := make([]byte, 20)

	_, err := io.ReadFull(
		conn,
		header,
	)

	if err != nil {
		return RPCRequest{}, err
	}

	req := RPCRequest{
		XID:       binary.BigEndian.Uint32(header[0:4]),
		Program:   binary.BigEndian.Uint32(header[4:8]),
		Version:   binary.BigEndian.Uint32(header[8:12]),
		Procedure: binary.BigEndian.Uint32(header[12:16]),
	}

	length := binary.BigEndian.Uint32(header[16:20])

	req.Args = make([]byte, length)

	_, err = io.ReadFull(
		conn,
		req.Args,
	)

	if err != nil {
		return RPCRequest{}, err
	}

	return req, nil
}

func sendRPCReply(
	conn net.Conn,
	xid uint32,
	result []byte,
) error {

	header := make([]byte, 8)

	binary.BigEndian.PutUint32(
		header[0:4],
		xid,
	)

	binary.BigEndian.PutUint32(
		header[4:8],
		uint32(len(result)),
	)

	if _, err := conn.Write(header); err != nil {
		return err
	}

	if _, err := conn.Write(result); err != nil {
		return err
	}

	return nil
}

func rpcCall(
	conn net.Conn,
	xid uint32,
	program uint32,
	version uint32,
	procedure uint32,
	args []byte,
) ([]byte, error) {

	header := make([]byte, 20)

	binary.BigEndian.PutUint32(
		header[0:4],
		xid,
	)

	binary.BigEndian.PutUint32(
		header[4:8],
		program,
	)

	binary.BigEndian.PutUint32(
		header[8:12],
		version,
	)

	binary.BigEndian.PutUint32(
		header[12:16],
		procedure,
	)

	binary.BigEndian.PutUint32(
		header[16:20],
		uint32(len(args)),
	)

	if _, err := conn.Write(header); err != nil {
		return nil, err
	}

	if _, err := conn.Write(args); err != nil {
		return nil, err
	}

	replyHeader := make([]byte, 8)

	if _, err := io.ReadFull(
		conn,
		replyHeader,
	); err != nil {
		return nil, err
	}

	replyXID := binary.BigEndian.Uint32(
		replyHeader[0:4],
	)

	length := binary.BigEndian.Uint32(
		replyHeader[4:8],
	)

	if replyXID != xid {

		return nil, fmt.Errorf(
			"XID mismatch: expected=%d got=%d",
			xid,
			replyXID,
		)
	}

	result := make([]byte, length)

	if _, err := io.ReadFull(
		conn,
		result,
	); err != nil {
		return nil, err
	}

	return result, nil
}
