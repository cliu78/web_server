web_server
==========

 a simple HTTP web server - an application that almost every company in the world runs. The web server should be able to respond to any page request, given the requested page is stored locally (i.e., on the same system in which the server is running). You can use any web browser (such as Firefox) as a client for my HTTP web server. 
 program can:  accepts connections from web browsers reads in the packets that the browsers send prepares and sends a response for the web browser from a local file

Since the web browser can only understand HTTP packets, this program use standard HTTP communicate format. 
When a web browser requests a page, it sends an HTTP request to the web server. 
The HTTP request has the following format

GET /index.html HTTP/1.1 
User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64; rv:11.0) Gecko/20100101 Firefox/11.0
Accept: */*
Connection: Keep-Alive

This HTTP packet notifies the web server of what file the web browser has requested. My web server will then serve the file to the web browser, which it will display on your screen.

All the program about server operations is in server.c. I used a dictionary and queue library that I coded in oher olde projects.

After downloading it, you need supply the port number when running my program. Since ports are shared globally on each system, it's important to choose a port number that someone else probably won't be using. Therefore, port numbers like 1234, 1111, or 777 are generally a bad idea since multiple people may choose to use the same port number. My program should be ran by using a command like the following (with a unique port number, of course):

./server 1234
Instead of interacting with the command line, you can use a web browser to connect to my server. Instructions to do this is at the bottom of this page.


I coded  a client to test it.

To test it

When you run the web server and the browser on the same machine, you can simply use: 
http://localhost:<port#>/


Running the web server:

To run your program, run the following commands:

%> make
%> ./server <port#> ...where <port#> is a port number. 
Note: 
1. When choosing your <port#>, choose a number above 1023 and below 60000. Making your number random (eg: not 1234, 2000, etc) will help avoid choosing a port used by another user. 
2. Since ports are shared globally, my bind() call will fail if someone else is already using same port. If this happens, wait a few seconds and then try again. If bind still fails, choose a new port.
