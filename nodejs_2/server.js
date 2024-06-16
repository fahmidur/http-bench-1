var cluster = require('cluster');
var numcpus = require('node:os').availableParallelism();

var path = require('path');
const http = require('node:http');

function print_usage() {
  var name = path.basename(process.argv[1]);
  console.log(`Usage: ${name} [OPTIONS]
      -addr string" << std::endl;
        The listening address (default \"127.0.0.1\")" << std::endl;
      -port int" << std::endl;
        The listening port (default 3535)" << std::endl;
  `)
}
console.log('process argv=', process.argv);

var addr = '127.0.0.1';
var port = 3535;
var expecting_port = false;
var expecting_addr = false;
for(var i = 0; i < process.argv.length; i++) {
  let arg = process.argv[i].replace(/^\-+/, '-')
  if(arg == '-help') {
    print_usage();
    process.exit(0);
  }
  else
  if(arg == '-port') {
    expecting_port = true;
  }
  else
  if(expecting_port) {
    expecting_port = false;
    port = parseInt(arg);
  }
  else
  if(arg == '-addr') {
    expecting_addr = true;
  }
  else
  if(expecting_addr) {
    expecting_addr = false;
    addr = arg;
  }
}

if(cluster.isPrimary) {
  console.log(`Primary ${process.pid} is running`);

  for(let i = 0; i < numcpus; i++) {
    cluster.fork();
  }

  cluster.on('exit', (worker, code, signal) => {
    console.log(`worker ${worker.process.pid} died`);
  });

} else {
  const server = http.createServer((req, res) => {
    if(req.url == '/time') {
      res.statusCode = 200;
      res.setHeader('Content-Type', 'text/html');
      res.end(`<html>
<head><title>Current time</title></head>
<body>
<h1>Current time</h1>
<p>The current time is ${Math.floor((new Date()).getTime() / 1000)} seconds since the epoch.</p>
</body>
</html>
  `);
    } else {
      res.statusCode = 404
      res.setHeader('Content-Type', 'text/plain');
      res.end('File not found');
    }
  });

  server.listen(port, addr, () => {
    console.log(`Server running at http://${addr}:${port}/`);
  });
}
