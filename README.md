# HTTP server
## DESIGN

Built a HTTP server using linux threads to accept multiple simultaneous connections and process them accordingly
The design uses the httpechosrv as a base to start with just echoing the messages onto the socket , code then adds fork() functionality to provide multiple simultanous connections. 
The requests are accepted ffrom the server are read from the socket in bunch of 16 bytes everytime, in a given sigle read cycle the 16 byte data are first parsedd and only then next chunk of data is read from the socket.
The parsing is done to find the 
    URL 	- /index.html (default)
    Method	- GET, POST
    Protocol	- HTTP/1.1

Further the Content-Length, Connection parameters are also parsed and saved into a structure of type request_t.
After the data is parsed each time, request is then passed to process_requestion function, which analyses the typr of method( GET, POST) and proceeds to analyzez the data accordingly. 
For GET request just the URL is exracted and contents are sent through the socket according to the requested content whether it is 
		.html	text/html
		.txt	text/plain
		.png	image/png
		.gif	image/gif
		.jpg	image/jpg
		.css	text/css
Depending upon the content type requested any of these headers are sent for the browser compatibility
For POST request the additional parameter Content-Length is used to let server know how many bytes of the data is required to be posted, It then reads the extra Content-Length number of bytes of data before processing the serving the request. After the data is collected the process_request adds the POST data using <pre> header and sends the requested data alonng with it.

HTTP parse uses a state machine design to parse the input data and keeps trach of the error to send back when the data parsing involves error.

For connecting to the server use localhost:<port number> from the browser or use telnet 127.0.0.1 <port number> from the terminal.

