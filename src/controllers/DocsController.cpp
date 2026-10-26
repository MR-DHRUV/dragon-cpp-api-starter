#include "api/controllers/DocsController.h"

namespace api::controllers {

static const char* kSwaggerUiHtml = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Backend Server API - Swagger Documentation</title>
    <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/swagger-ui-dist@5.18.2/swagger-ui.css" />
    <link rel="icon" type="image/png" href="https://cdn.jsdelivr.net/npm/swagger-ui-dist@5.18.2/favicon-32x32.png" />
    <style>
        html { box-sizing: border-box; overflow-y: scroll; }
        *, *:before, *:after { box-sizing: inherit; }
        body { margin: 0; background: #fafafa; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; }
        .swagger-ui .topbar { display: none; }
        .custom-header {
            background: #1f2937;
            color: #ffffff;
            padding: 16px 32px;
            display: flex;
            align-items: center;
            justify-content: space-between;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
        .custom-header h1 {
            margin: 0;
            font-size: 20px;
            font-weight: 600;
            letter-spacing: 0.5px;
        }
        .custom-header .badge {
            background: #10b981;
            color: white;
            padding: 4px 10px;
            border-radius: 9999px;
            font-size: 12px;
            font-weight: bold;
        }
    </style>
</head>
<body>
    <header class="custom-header">
        <h1>REST API Documentation</h1>
        <span class="badge">OpenAPI 3.0</span>
    </header>
    <div id="swagger-ui"></div>
    <script src="https://cdn.jsdelivr.net/npm/swagger-ui-dist@5.18.2/swagger-ui-bundle.js"></script>
    <script src="https://cdn.jsdelivr.net/npm/swagger-ui-dist@5.18.2/swagger-ui-standalone-preset.js"></script>
    <script>
    window.onload = function() {
        window.ui = SwaggerUIBundle({
            url: "/api/v1/openapi.json",
            dom_id: '#swagger-ui',
            deepLinking: true,
            presets: [
                SwaggerUIBundle.presets.apis,
                SwaggerUIStandalonePreset
            ],
            plugins: [
                SwaggerUIBundle.plugins.DownloadUrl
            ],
            layout: "StandaloneLayout",
            displayRequestDuration: true,
            docExpansion: "list",
            filter: true
        });
    };
    </script>
</body>
</html>)rawhtml";

static const char* kOpenApiJson = R"rawjson({
  "openapi": "3.0.3",
  "info": {
    "title": "Backend Server REST API",
    "description": "Asynchronous REST API built with modern C++20, Drogon, RapidJSON, and PostgreSQL, integrated with OpenTelemetry (traces, metrics, logs).",
    "version": "1.0.0",
    "contact": {
      "name": "Backend Engineering Team"
    }
  },
  "servers": [
    {
      "url": "http://localhost:8080",
      "description": "Local development server"
    }
  ],
  "tags": [
    {
      "name": "Users",
      "description": "User CRUD management and username availability verification"
    },
    {
      "name": "Health & Probes",
      "description": "System readiness, uptime, and database latency health probes"
    },
    {
      "name": "Observability",
      "description": "Prometheus metric scraping and OpenTelemetry telemetry"
    }
  ],
  "paths": {
    "/api/v1/ping": {
      "get": {
        "tags": ["Health & Probes"],
        "summary": "Microsecond latency ping probe",
        "description": "Returns immediate pong response with server UNIX millisecond timestamp for network heartbeat checks.",
        "responses": {
          "200": {
            "description": "Ping succeeded",
            "content": {
              "application/json": {
                "schema": {
                  "$ref": "#/components/schemas/PingResponse"
                }
              }
            }
          }
        }
      }
    },
    "/api/v1/health": {
      "get": {
        "tags": ["Health & Probes"],
        "summary": "Deep system and PostgreSQL healthcheck",
        "description": "Probes live PostgreSQL connection, measures database ping latency, reports active thread count and uptime.",
        "responses": {
          "200": {
            "description": "System and database are healthy",
            "content": {
              "application/json": {
                "schema": {
                  "$ref": "#/components/schemas/HealthResponse"
                }
              }
            }
          },
          "503": {
            "description": "Database connection degraded or unavailable",
            "content": {
              "application/json": {
                "schema": {
                  "$ref": "#/components/schemas/HealthResponse"
                }
              }
            }
          }
        }
      }
    },
    "/api/v1/metrics": {
      "get": {
        "tags": ["Observability"],
        "summary": "Prometheus metrics exposition endpoint",
        "description": "Exposes standard Prometheus formatted metrics including request counters, latency histogram buckets, and database query statistics.",
        "responses": {
          "200": {
            "description": "Prometheus metrics exposition text format",
            "content": {
              "text/plain": {
                "schema": {
                  "type": "string",
                  "example": "# HELP http_requests_total Total number of HTTP requests processed\n# TYPE http_requests_total counter\nhttp_requests_total{method=\"GET\",route=\"/api/v1/users\",status=\"200\"} 42\n"
                }
              }
            }
          }
        }
      }
    },
    "/api/v1/users": {
      "get": {
        "tags": ["Users"],
        "summary": "Retrieve paginated list of users",
        "description": "Fetches users from PostgreSQL with configurable limit and offset.",
        "parameters": [
          {
            "name": "limit",
            "in": "query",
            "description": "Maximum number of users to retrieve (1 to 200)",
            "required": false,
            "schema": {
              "type": "integer",
              "default": 50,
              "minimum": 1,
              "maximum": 200
            }
          },
          {
            "name": "offset",
            "in": "query",
            "description": "Number of records to skip for pagination",
            "required": false,
            "schema": {
              "type": "integer",
              "default": 0,
              "minimum": 0
            }
          }
        ],
        "responses": {
          "200": {
            "description": "List of users successfully retrieved",
            "content": {
              "application/json": {
                "schema": {
                  "$ref": "#/components/schemas/UserListResponse"
                }
              }
            }
          }
        }
      },
      "post": {
        "tags": ["Users"],
        "summary": "Create a new user",
        "description": "Validates username format and availability, verifies email uniqueness, and inserts user into PostgreSQL.",
        "requestBody": {
          "required": true,
          "content": {
            "application/json": {
              "schema": {
                "$ref": "#/components/schemas/CreateUserRequest"
              }
            }
          }
        },
        "responses": {
          "201": {
            "description": "User created successfully",
            "content": {
              "application/json": {
                "schema": {
                  "$ref": "#/components/schemas/UserResponse"
                }
              }
            }
          },
          "400": {
            "description": "Validation error (invalid format, length, or missing field)"
          },
          "409": {
            "description": "Conflict (username or email already taken)"
          }
        }
      }
    },
    "/api/v1/users/{id}": {
      "get": {
        "tags": ["Users"],
        "summary": "Retrieve user by ID",
        "description": "Fetches a single user record by numeric identifier.",
        "parameters": [
          {
            "name": "id",
            "in": "path",
            "required": true,
            "description": "User ID",
            "schema": {
              "type": "integer",
              "format": "int64"
            }
          }
        ],
        "responses": {
          "200": {
            "description": "User record retrieved",
            "content": {
              "application/json": {
                "schema": {
                  "$ref": "#/components/schemas/UserResponse"
                }
              }
            }
          },
          "404": {
            "description": "User not found"
          }
        }
      },
      "put": {
        "tags": ["Users"],
        "summary": "Update an existing user",
        "description": "Updates full name, email, active status, or bio of an existing user.",
        "parameters": [
          {
            "name": "id",
            "in": "path",
            "required": true,
            "description": "User ID",
            "schema": {
              "type": "integer",
              "format": "int64"
            }
          }
        ],
        "requestBody": {
          "required": true,
          "content": {
            "application/json": {
              "schema": {
                "$ref": "#/components/schemas/UpdateUserRequest"
              }
            }
          }
        },
        "responses": {
          "200": {
            "description": "User updated successfully",
            "content": {
              "application/json": {
                "schema": {
                  "$ref": "#/components/schemas/UserResponse"
                }
              }
            }
          },
          "404": {
            "description": "User not found"
          }
        }
      },
      "delete": {
        "tags": ["Users"],
        "summary": "Delete a user by ID",
        "description": "Removes the user record from PostgreSQL.",
        "parameters": [
          {
            "name": "id",
            "in": "path",
            "required": true,
            "description": "User ID",
            "schema": {
              "type": "integer",
              "format": "int64"
            }
          }
        ],
        "responses": {
          "200": {
            "description": "User deleted successfully"
          },
          "404": {
            "description": "User not found"
          }
        }
      }
    },
    "/api/v1/users/availability/{username}": {
      "get": {
        "tags": ["Users"],
        "summary": "Check username availability (Path parameter)",
        "description": "Verifies format (3-30 chars, alphanumeric + underscore) and checks if already registered in PostgreSQL.",
        "parameters": [
          {
            "name": "username",
            "in": "path",
            "required": true,
            "description": "Username to check",
            "schema": {
              "type": "string"
            }
          }
        ],
        "responses": {
          "200": {
            "description": "Availability check result",
            "content": {
              "application/json": {
                "schema": {
                  "$ref": "#/components/schemas/UsernameAvailabilityResponse"
                }
              }
            }
          },
          "400": {
            "description": "Invalid username format"
          }
        }
      }
    },
    "/api/v1/users/check-username": {
      "get": {
        "tags": ["Users"],
        "summary": "Check username availability (Query parameter)",
        "description": "Query parameter variant for frontend live registration forms (e.g. `?username=alex_turner`).",
        "parameters": [
          {
            "name": "username",
            "in": "query",
            "required": true,
            "description": "Username to check",
            "schema": {
              "type": "string"
            }
          }
        ],
        "responses": {
          "200": {
            "description": "Availability check result",
            "content": {
              "application/json": {
                "schema": {
                  "$ref": "#/components/schemas/UsernameAvailabilityResponse"
                }
              }
            }
          },
          "400": {
            "description": "Invalid username format"
          }
        }
      }
    }
  },
  "components": {
    "schemas": {
      "User": {
        "type": "object",
        "properties": {
          "id": { "type": "integer", "format": "int64", "example": 1 },
          "username": { "type": "string", "example": "alex_turner" },
          "email": { "type": "string", "format": "email", "example": "alex.turner@example.com" },
          "fullName": { "type": "string", "example": "Alex Turner" },
          "bio": { "type": "string", "example": "Software Engineer" },
          "isActive": { "type": "boolean", "example": true },
          "createdAt": { "type": "string", "format": "date-time", "example": "2026-10-20T00:00:00Z" },
          "updatedAt": { "type": "string", "format": "date-time", "example": "2026-10-20T00:00:00Z" }
        }
      },
      "CreateUserRequest": {
        "type": "object",
        "required": ["username", "email", "fullName"],
        "properties": {
          "username": { "type": "string", "minLength": 3, "maxLength": 30, "example": "alex_turner" },
          "email": { "type": "string", "format": "email", "example": "alex.turner@example.com" },
          "fullName": { "type": "string", "example": "Alex Turner" },
          "bio": { "type": "string", "example": "Backend developer and distributed systems enthusiast" }
        }
      },
      "UpdateUserRequest": {
        "type": "object",
        "properties": {
          "email": { "type": "string", "format": "email", "example": "alex.updated@example.com" },
          "fullName": { "type": "string", "example": "Alex Turner (Updated)" },
          "isActive": { "type": "boolean", "example": true },
          "bio": { "type": "string", "example": "Lead engineer" }
        }
      },
      "UserResponse": {
        "type": "object",
        "properties": {
          "success": { "type": "boolean", "example": true },
          "status": { "type": "integer", "example": 200 },
          "message": { "type": "string", "example": "User retrieved successfully" },
          "data": { "$ref": "#/components/schemas/User" }
        }
      },
      "UserListResponse": {
        "type": "object",
        "properties": {
          "success": { "type": "boolean", "example": true },
          "status": { "type": "integer", "example": 200 },
          "message": { "type": "string", "example": "Users retrieved successfully" },
          "data": {
            "type": "array",
            "items": { "$ref": "#/components/schemas/User" }
          }
        }
      },
      "UsernameAvailabilityData": {
        "type": "object",
        "properties": {
          "username": { "type": "string", "example": "alex_turner" },
          "available": { "type": "boolean", "example": true },
          "valid": { "type": "boolean", "example": true },
          "reason": { "type": "string", "example": "Username is available" }
        }
      },
      "UsernameAvailabilityResponse": {
        "type": "object",
        "properties": {
          "success": { "type": "boolean", "example": true },
          "status": { "type": "integer", "example": 200 },
          "message": { "type": "string", "example": "Username is available" },
          "data": { "$ref": "#/components/schemas/UsernameAvailabilityData" }
        }
      },
      "PingResponse": {
        "type": "object",
        "properties": {
          "pong": { "type": "boolean", "example": true },
          "timestamp": { "type": "integer", "format": "int64", "example": 1792437649727 }
        }
      },
      "HealthResponse": {
        "type": "object",
        "properties": {
          "success": { "type": "boolean", "example": true },
          "status": { "type": "integer", "example": 200 },
          "message": { "type": "string", "example": "System health report" },
          "data": {
            "type": "object",
            "properties": {
              "status": { "type": "string", "example": "UP" },
              "uptimeSeconds": { "type": "integer", "example": 360 },
              "threads": { "type": "integer", "example": 4 },
              "database": {
                "type": "object",
                "properties": {
                  "connected": { "type": "boolean", "example": true },
                  "latencyMs": { "type": "number", "example": 0.42 },
                  "detail": { "type": "string", "example": "PostgreSQL connection healthy" }
                }
              }
            }
          }
        }
      }
    }
  }
})rawjson";

void DocsController::getSwaggerUi(
    const drogon::HttpRequestPtr& /*req*/,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k200OK);
    resp->setContentTypeCode(drogon::CT_TEXT_HTML);
    resp->setBody(kSwaggerUiHtml);
    callback(resp);
}

void DocsController::getOpenApiJson(
    const drogon::HttpRequestPtr& /*req*/,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k200OK);
    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    resp->setBody(kOpenApiJson);
    callback(resp);
}

} // namespace api::controllers
