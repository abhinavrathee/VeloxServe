# VeloxServe — Task Tracker

## 🟢 Phase 1: Core Server
- [x] `CMakeLists.txt` — Build system (C++17, pthreads)
- [x] `include/core/connection.h` — Connection state struct
- [x] `include/core/epoll_wrapper.h` — Epoll RAII wrapper header
- [x] `src/core/epoll_wrapper.cpp` — Epoll implementation
- [x] `include/core/thread_pool.h` — Thread pool header
- [x] `src/core/thread_pool.cpp` — Thread pool implementation
- [x] `include/core/server.h` — Main server header
- [x] `src/core/server.cpp` — Server implementation (accept/read/write/close)
- [x] `src/main.cpp` — Entry point with signal handling
- [x] `www/index.html` — Test landing page
- [x] `www/errors/404.html` — Error page
- [x] `www/errors/500.html` — Error page
- [x] **BUILD TEST** — Compile on Linux/WSL (Or Docker!)
- [x] **CURL TEST** — `curl localhost:8080` returns "Hello from VeloxServe!"

## 🟡 Phase 2: HTTP Parser + Static Files
- [x] `include/http/http_parser.h`
- [x] `src/http/http_parser.cpp`
- [x] `include/http/http_response.h`
- [x] `src/http/http_response.cpp`
- [x] `include/http/mime_types.h`
- [x] `src/http/mime_types.cpp`
- [x] `include/modules/static_handler.h`
- [x] `src/modules/static_handler.cpp`
- [x] Update `server.cpp` to use parser + static handler

## 🟠 Phase 3: Router
- [x] `include/http/router.h`
- [x] `src/http/router.cpp`
- [x] Wire router into server

## 🔵 Phase 4: Config Parser
- [x] `include/config/config_parser.h`
- [x] `src/config/config_parser.cpp`
- [x] `include/config/server_config.h`
- [x] `include/config/location_config.h`
- [x] `veloxserve.conf`
- [x] Wire config into server startup

## 🔴 Phase 5: Reverse Proxy
- [x] `include/modules/proxy_handler.h`
- [x] `src/modules/proxy_handler.cpp`
- [x] Wire proxy into router

## 🟣 Phase 6: Load Balancer
- [x] Upstream config support

## 🟤 Phase 7: Rate Limiter + Cache
- [x] `include/middleware/rate_limiter.h`
- [x] `src/middleware/rate_limiter.cpp`
- [x] `include/middleware/cache.h`
- [x] `src/middleware/cache.cpp`

## ⚫ Phase 8: Logging + Metrics + Docker
- [x] `include/middleware/logger.h`
- [x] `src/middleware/logger.cpp`
- [x] `/health` and `/metrics` endpoints
- [x] `Dockerfile`
- [x] `docker-compose.yml`
- [x] Final benchmarks with `wrk`
