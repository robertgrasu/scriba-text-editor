package main

import "fmt"

type Server struct {
	Name string
	Port int
}

func (s *Server) Start() error {
	fmt.Println("starting", s.Name, 8080)
	return nil
}
