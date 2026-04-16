./build/demos/http_server_demo 
OmniNet HTTP Server Demo
========================
[Demo] Registering route: GET /api/hello
[Demo] Registering route: GET /api/time
[Demo] Registering route: GET /api/health
[Demo] Registering route: POST /api/echo
[Demo] Registering static route: GET /
[Demo] Registering static route: GET /* (catch-all)
Starting server on 127.0.0.1:8080
Serving static files from: "/home/lee/projects/Programming/projects_C++/OmniNet/static"
[HttpServer] Starting server on 127.0.0.1:8080
[HttpServer] Socket bound and listening
[HttpServer] Starting 4 worker threads
[HttpServer] Worker threads started, entering accept loop
[HttpServer] Accept loop started
[HttpServer] Waiting for connection...
Server started!
Try these endpoints:
  http://localhost:8080/api/hello
  http://localhost:8080/api/health
  http://localhost:8080/api/time

Press Ctrl+C to stop the server...
[HttpServer] Connection accepted! Spawning handler thread
[HttpServer] handleConnection called, socket fd=4
[HttpServer] Waiting for connection...
[HttpServer] Connection thread started, socket fd=4
[HttpServer] Calling conn.processRequests()
[HttpConnection] handleNextRequest() called
[HttpConnection] readRequest() starting
[HttpConnection]   Reading chunk, buffer size=0
[HttpConnection]   Received 455 bytes
[HttpConnection]   Found double CRLF at position 451
[HttpConnection]   Header end at position 455
[HttpConnection]   Parsing headers: 455 bytes
[HttpConnection]   Parse consumed 455 bytes
[HttpConnection]   Content-Length: 0
[HttpConnection]   No body needed, returning true
[HttpConnection] readRequest() succeeded - path: /api/hello
[HttpConnection] Calling handler for path: /api/hello
[HttpServer] Handler called - Method: GET, Path: /api/hello
[HttpServer] Calling router.handle()
[Router] handle() called - Method: GET, Path: /api/hello
[Router] Registered routes: 6
[Router]   GET /api/hello
[Router]   GET /api/time
[Router]   GET /api/health
[Router]   POST /api/echo
[Router]   GET /
[Router]   GET /*
[Router] Looking for matching route...
[Router] match() - Trying to match /api/hello with method GET
[Router]   Checking route: GET /api/hello
[Router]     Method matches, testing regex...
[Router]     regex_match result: MATCH
[Router]     MATCHED! captures=0
[Router] Route MATCHED! Calling handler
[Demo] Handler called for /api/hello
[Router] Handler returned, response status=200
[HttpServer] Router.handle() returned, response status=200
[HttpConnection] Sending response, status=200
[HttpConnection] sendResponse() called, status=200
[HttpConnection] buildFull returned 99 bytes
[HttpConnection] Response preview: HTTP/1.1 200 OK
Content-Length: 28
Content-Type: application/json

{"message": "Hello, World!"}
[HttpConnection] Calling socket->sendAll()
[HttpConnection] sendAll() succeeded
[HttpConnection] sendResponse() returning 1, keepAlive=1
[HttpConnection] handleNextRequest() called
[HttpConnection] readRequest() starting
[HttpConnection]   Reading chunk, buffer size=0
[HttpServer] Connection accepted! Spawning handler thread
[HttpServer] handleConnection called, socket fd=5
[HttpServer] Waiting for connection...
[HttpServer] Connection thread started, socket fd=5
[HttpServer] Calling conn.processRequests()
[HttpConnection] handleNextRequest() called
[HttpConnection] readRequest() starting
[HttpConnection]   Reading chunk, buffer size=0
[HttpConnection]   Received 496 bytes
[HttpConnection]   Found double CRLF at position 492
[HttpConnection]   Header end at position 496
[HttpConnection]   Parsing headers: 496 bytes
[HttpConnection]   Parse consumed 496 bytes
[HttpConnection]   Content-Length: 0
[HttpConnection]   No body needed, returning true
[HttpConnection] readRequest() succeeded - path: /favicon.ico
[HttpConnection] Calling handler for path: /favicon.ico
[HttpServer] Handler called - Method: GET, Path: /favicon.ico
[HttpServer] Calling router.handle()
[Router] handle() called - Method: GET, Path: /favicon.ico
[Router] Registered routes: 6
[Router]   GET /api/hello
[Router]   GET /api/time
[Router]   GET /api/health
[Router]   POST /api/echo
[Router]   GET /
[Router]   GET /*
[Router] Looking for matching route...
[Router] match() - Trying to match /favicon.ico with method GET
[Router]   Checking route: GET /api/hello
[Router]     Method matches, testing regex...
[Router]     regex_match result: NO MATCH
[Router]   Checking route: GET /api/time
[Router]     Method matches, testing regex...
[Router]     regex_match result: NO MATCH
[Router]   Checking route: GET /api/health
[Router]     Method matches, testing regex...
[Router]     regex_match result: NO MATCH
[Router]   Checking route: POST /api/echo
[Router]     Method mismatch, skipping
[Router]   Checking route: GET /
[Router]     Method matches, testing regex...
[Router]     regex_match result: NO MATCH
[Router]   Checking route: GET /*
[Router]     Method matches, testing regex...
[Router]     regex_match result: MATCH
[Router]     MATCHED! captures=0
[Router] Route MATCHED! Calling handler
[Demo] Static handler for /*
[Router] Handler returned, response status=200
[HttpServer] Router.handle() returned, response status=200
[HttpConnection] Sending response, status=200
[HttpConnection] sendResponse() called, status=200
[HttpConnection] buildFull returned 19 bytes
[HttpConnection] Response preview: HTTP/1.1 200 OK


[HttpConnection] Calling socket->sendAll()
[HttpConnection] sendAll() succeeded
[HttpConnection] sendResponse() returning 1, keepAlive=1
[HttpConnection] handleNextRequest() called
[HttpConnection] readRequest() starting
[HttpConnection]   Reading chunk, buffer size=0
Terminated                 ./build/demos/http_server_demo