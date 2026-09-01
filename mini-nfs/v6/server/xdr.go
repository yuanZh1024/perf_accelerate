package main

import (
	"bytes"
	"encoding/binary"
	"fmt"
)

func xdrUint32(v uint32) []byte {

	buf := make([]byte, 4)

	binary.BigEndian.PutUint32(buf, v)

	return buf
}

func xdrUint64(v uint64) []byte {

	buf := make([]byte, 8)

	binary.BigEndian.PutUint64(buf, v)

	return buf
}

func xdrOpaque(data []byte) []byte {

	var buf bytes.Buffer

	// length
	buf.Write(xdrUint32(uint32(len(data))))

	// data
	buf.Write(data)

	// padding
	padding := (4 - len(data)%4) % 4

	buf.Write(make([]byte, padding))

	return buf.Bytes()
}

func xdrString(s string) []byte {
	return xdrOpaque([]byte(s))
}

type XDRReader struct {
	data []byte
	pos  int
}

func NewXDRReader(data []byte) *XDRReader {
	return &XDRReader{
		data: data,
	}
}

func (r *XDRReader) Uint32() (uint32, error) {

	if r.pos+4 > len(r.data) {
		return 0, fmt.Errorf("XDR uint32 overflow")
	}

	v := binary.BigEndian.Uint32(
		r.data[r.pos : r.pos+4],
	)

	r.pos += 4

	return v, nil
}

func (r *XDRReader) Uint64() (uint64, error) {

	if r.pos+8 > len(r.data) {
		return 0, fmt.Errorf("XDR uint64 overflow")
	}

	v := binary.BigEndian.Uint64(
		r.data[r.pos : r.pos+8],
	)

	r.pos += 8

	return v, nil
}

func (r *XDRReader) Opaque() ([]byte, error) {

	length, err := r.Uint32()

	if err != nil {
		return nil, err
	}

	n := int(length)

	if r.pos+n > len(r.data) {
		return nil, fmt.Errorf("XDR opaque overflow")
	}

	data := r.data[
		r.pos : r.pos+n,
	]

	r.pos += n

	padding := (4 - n%4) % 4

	if r.pos+padding > len(r.data) {
		return nil, fmt.Errorf("XDR padding overflow")
	}

	r.pos += padding

	return data, nil
}
