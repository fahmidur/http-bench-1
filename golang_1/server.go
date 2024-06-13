package main

import (
	"flag"
	"fmt"
	"net/http"
	"os"
	"strconv"
	"time"
)

func http_time(w http.ResponseWriter, req *http.Request) {
	w.Header().Set("Content-Type", "text/html")
	now := time.Now()
	fmt.Fprintf(
		w,
		"<html>\n"+
			"<head><title>Current time</title></head>\n"+
			"<body>\n"+
			"<h1>Current time</h1>\n"+
			"<p>The current time is "+
			"%d"+
			" seconds since the epoch.</p>\n"+
			"</body>\n"+
			"</html>\n",
		now.Unix(),
	)
}
func main() {
	var addrPtr = flag.String("addr", "127.0.0.1", "The listening address")
	var portPtr = flag.Int("port", 3535, "The listening port")
	flag.Parse()

	if *addrPtr == "" {
		fmt.Println("ERROR: expecting arg 'addr' for address")
		os.Exit(1)
	}

	if *portPtr < 0 || *portPtr > 65535 {
		fmt.Println("ERROR: invalid port")
		os.Exit(1)
	}

	http.HandleFunc("/time", http_time)
	address_port := *addrPtr + ":" + strconv.Itoa(*portPtr)
	fmt.Println("Starting server on http://" + address_port)
	http.ListenAndServe(address_port, nil)
}
