# HTTP webserver
Single-threaded IO multiplexing HTTP web server written in C++.

### Features
- GET, HEAD, POST, PUT, DELETE **HTTP methods**.
- static HTML files and **CGI**.
- **Configuration** rules similar to nginx. It includes `alias`, `auto_index`, `cgi_pass`, `error_page`, `limit_except`, `location`, `return` along with essentials such as `index`, `listen`, `root`, `server_name`.
- Virtual server that allows exposing multiple servers on same `IP:PORT` pair. Uses HTTP host name field to find servers.
- Multi-platform(Linux, macOS). It have been written mostly using UNIX system calls and C++ standard library.

### Debug
- `ERROR, WARNING, INFO, DEBUG` logging levels.
- Debug build(`make debug`) that enables `DEBUG` logging and compiler sanitizer.

# Build
C++ compiler and GNU Make are needed for build.

### Command
There are `make release` and `make debug`. Use `make debug` to enable debug features.

`make` command follows previous one. Using `make` after `make debug` will start debug build. Use `make release` or `make fclean` to reset the build mode to release.

# Config

Servers are defined at the configuration file. `config` directory contains examples.
See [nginx documentation](https://nginx.org/en/docs/) to learn detail.

### Overview
```
server {
    server_name mydomain.com;

	root html;

	index	index.php;

	listen	8080;

	cgi_pass .php cgi-bin/php-cgi;

	location /resource/ {

		limit_except GET;

		client_max_body_size 100;

		alias tester_dir/YoupiBanane/;
	}
}
```

- `server { server_definition }`

A server block will initialize a virtual server according to the server definition.

- `server_name`

Set the name of a virtual server. If multiple servers share same IP or port, they are identified by examining the server name.

- `root path`

Set the root path for URL-to-path mapping.

For example, `http://mydomain.com/hello/world` URL maps into `$WEBSERV_ROOT/html/hello/world` path. The domain part of the URL(`mydomain.com`) will be translated to `$WEBSERV_ROOT/html` and the path part(`/hello/world`) is appended there to generate complete file path.

If `path` starts with `/`, `$WEBSERV_ROOT` is not used(i.e. `path` is used as absolute path).

- `index file`

Without index file, `http://mydomain.com/` cannot be mapped to a file because path part is `/` and it will map to root path.
Index file is `index.html` by default.

- `listen (port|addr|addr:port)`

Address passed to listening socket of the server.

- `cgi_pass postfix executable_path`

If the path part ends with `postfix`(e.g. mydomain.com/index.php), server will spawn CGI by executing `$WEBSERV_ROOT/executable_path` and communicate with it to acquire the body of HTTP response.

- `autoindex`

If the path part is not a file and there's no index file, respond with autoindex which is a generated HTML file for path traversal.

- `return address`

Respond with 301 redirect.

- `error_page status_code file`

Respond with `status_code` and content of `file` as body.

- `client_max_body_size integer`

Limit the size of the body of HTTP request. `INT32_MAX` by default.

#### Location
`location path { location_definition }`

Maps a specific path to `location` which can have its own `index`, `root`, `autoindex`, `return`. It will inherit other options from the `server`.

- `alias`

Instead of using `root` of `server`, assign a independent path. Starting `/` matters like `root`.

- `limit_except method( method)*`

Limit the HTTP method(s) that location can receive.


# Execute
It requires a configuration file to execute.

### Option
Use `--log=[level]` command line argument to adjust logging level. Default is `INFO`.
`ERROR`, `WARNING`, `INFO`, `DEBUG` are available(case sensitive).

# HTTP
we have implemented subset of server-side HTTP/1.1, priotizing **MUST** and **SHALL** requirements such as HTTP message structure, ``Content-Length``, status code and etc.
