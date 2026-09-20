# OmniNet Routing Guide

## Quick Reference

OmniNet uses **first-match-wins** routing. Routes are matched in registration order.

### Golden Rule

> **Register specific routes BEFORE catch-all routes.**

---

## Route Registration Order

### ✅ Correct Order

```cpp
// 1. First: Specific API routes
server.get("/api/v1/accounts", handleAccounts);
server.get("/api/v1/users", handleUsers);

// 2. Second: Static routes
server.get("/static/*", staticHandler);

// 3. Last: Catch-all (fallback)
server.get("/*", handle404);
```

### ❌ Wrong Order (Common Bug)

```cpp
// WRONG: Catch-all first shadows everything!
server.get("/*", handleStatic);          // Catches ALL requests
server.get("/api/v1/accounts", handle);  // NEVER REACHED
```

---

## Route Patterns

| Pattern | Matches | Example |
|---------|---------|---------|
| `/exact` | Exact path only | `/api/users` matches only `/api/users` |
| `/:param` | Single segment | `/:id` matches `/123` but not `/123/456` |
| `/:parent/:child` | Multiple params | `/users/:id/posts/:postId` |
| `/path/*` | Path and subpaths | `/api/*` matches `/api/v1/accounts` |

### Examples

```cpp
// Exact match
server.get("/api/health", [](auto& req, auto& resp) {
    resp.setBody("OK");
});

// Parameter capture
server.get("/users/:id", [](auto& req, auto& resp) {
    std::string id(req.pathParam(0));  // "123" for /users/123
    resp.setBody("User: " + id);
});

// Multiple parameters
server.get("/api/:version/:resource/:id", [](auto& req, auto& resp) {
    std::string version(req.pathParam(0));   // "v1"
    std::string resource(req.pathParam(1));  // "accounts"
    std::string id(req.pathParam(2));        // "123"
});

// Wildcard catch-all
server.get("/files/*", [](auto& req, auto& resp) {
    std::string path(req.path());  // Full path including after /
    resp.setBody("File: " + path);
});
```

---

## Common Issues

### Issue 1: Catch-all Shadows Specific Routes

```cpp
// This setup never matches /api/v1/accounts
server.get("/*", staticHandler);          // WRONG: Registered first
server.get("/api/v1/accounts", handler);  // Never reached
```

**Fix:**
```cpp
server.get("/api/v1/accounts", handler);  // First
server.get("/*", staticHandler);          // Last
```

### Issue 2: Generic Pattern Before Specific

```cpp
// /api/v1/users never matches because /api/* catches first
server.get("/api/*", genericHandler);     // WRONG
server.get("/api/v1/users", specificHandler);
```

**Fix:**
```cpp
server.get("/api/v1/users", specificHandler);  // More specific first
server.get("/api/*", genericHandler);
```

### Issue 3: Route Group Order

When using `router.group()`, the group is registered when the callback runs:

```cpp
// Group registered first (when callback executes)
router.group("/api", [](Router& r) {
    r.get("/users", handler);  // Pattern: /api/users
});

// This is registered after, so it won't override the group route
router.get("/api/users", overrideHandler);  // Won't match if /api/users exists
```

---

## Debugging Routes

### Print All Registered Routes

```cpp
#include <iostream>

auto routes = server.router().routes();
std::cout << "Registered routes (" << routes.size() << "):" << std::endl;
for (size_t i = 0; i < routes.size(); i++) {
    std::cout << "  " << i << ": " << routes[i].pattern << std::endl;
}
```

### Test Route Matching

```cpp
#include "http/Router.h"

void testMatching() {
    OmniNet::HTTP::Router router;
    router.get("/api/v1/accounts", [](auto& req, auto& resp) {});
    router.get("/*", [](auto& req, auto& resp) {});
    
    // Test a specific path
    auto match = router.match(
        OmniNet::HTTP::Method::GET, 
        "/api/v1/accounts"
    );
    
    if (match) {
        std::cout << "Matched! Captures: " << match->captures.size() << std::endl;
    } else {
        std::cout << "No match found" << std::endl;
    }
}
```

---

## Best Practices

1. **Always register specific routes before catch-all routes**
2. **Use `:param` instead of wildcards when possible**
3. **Test route matching during development**
4. **Log which route is handling requests** when debugging
5. **Keep route registration in a single place** (e.g., setup function)

---

## Method Support

| Method | Function | Description |
|--------|----------|-------------|
| GET | `server.get("/path", handler)` | Read data |
| POST | `server.post("/path", handler)` | Create resource |
| PUT | `server.put("/path", handler)` | Replace resource |
| PATCH | `server.patch("/path", handler)` | Update resource |
| DELETE | `server.delete_("/path", handler)` | Delete resource |
| HEAD | `server.head("/path", handler)` | Headers only |
| OPTIONS | `server.options("/path", handler)` | CORS preflight |
| ANY | `server.any("/path", handler)` | All methods |

---

## Trailing Slash Redirects

With `setRedirectTrailingSlash(true)` (the default), the router canonicalises
trailing slashes with a redirect instead of returning 404:

| Request                | Matching route | Response |
|------------------------|----------------|----------|
| `GET /blog/`           | `/blog`        | `301` → `/blog` |
| `GET /blog/?page=1`    | `/blog`        | `301` → `/blog?page=1` (query preserved) |
| `GET /blog/hello/`     | `/blog/:slug`  | `301` → `/blog/hello` |
| `GET /docs`            | `/docs/`       | `301` → `/docs/` |
| `POST /contact/`       | `/contact`     | `308` → `/contact` (method and body preserved) |

Notes:

- Both directions are tested against the route's compiled pattern, so
  parameterised routes redirect correctly.
- `GET`/`HEAD` requests receive **301 Moved Permanently**; all other methods
  receive **308 Permanent Redirect** so the method and body survive
  (clients historically rewrite 301/302 POSTs to GET).
- The root path `/` is never redirected.
- Redirects fire before any middleware runs.
- Disable with `router.setRedirectTrailingSlash(false)`; unmatched slashed
  paths then return 404.

## Related Documentation

- [HTTP Server API](./http-server.md) - Full server documentation
- [Route Parameters](./http-server.md#route-parameters) - Parameter extraction
- [Admin Endpoints](./http-server.md#admin-endpoints) - Built-in `/omninet/*` routes