.PHONY: help dev build run test test-unit test-api db stop-db migrate migrate-up migrate-down clean

BUILD_DIR  ?= build
BUILD_TYPE ?= Release

# Auto-detect VCPKG_ROOT from env or PATH
ifeq ($(VCPKG_ROOT),)
  VCPKG_CMD := $(shell where vcpkg 2>nul || which vcpkg 2>/dev/null)
  ifneq ($(VCPKG_CMD),)
    VCPKG_ROOT := $(patsubst %/,%,$(dir $(VCPKG_CMD)))
  endif
endif

# Locate CMake: prefer 'cmake' in PATH, fall back to detected installations on Windows
CMAKE ?= $(firstword $(shell where cmake 2>nul || which cmake 2>/dev/null))
ifeq ($(CMAKE),)
  ifeq ($(shell if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" echo YES),YES)
    CMAKE := "C:/Program Files/Microsoft Visual Studio/2022/Professional/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
  else ifneq ($(VCPKG_ROOT),)
    ifeq ($(shell if exist "$(VCPKG_ROOT)\downloads\tools\cmake-3.30.1-windows\cmake-3.30.1-windows-i386\bin\cmake.exe" echo YES),YES)
      CMAKE := "$(VCPKG_ROOT)/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/bin/cmake.exe"
    else
      CMAKE := cmake
    endif
  else
    CMAKE := cmake
  endif
endif

CMAKE_FLAGS := -B $(BUILD_DIR) -S . -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
ifneq ($(VCPKG_ROOT),)
  CMAKE_FLAGS += -DCMAKE_TOOLCHAIN_FILE=$(VCPKG_ROOT)/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows -DVCPKG_MANIFEST_MODE=OFF
endif

help:
	@echo =========================================================
	@echo                 Backend Server Commands
	@echo =========================================================
	@echo   make dev        - Build backend server and run
	@echo   make build      - Configure and build backend server ($(BUILD_TYPE))
	@echo   make run        - Run the compiled backend server
	@echo   make test       - Run all unit and integration tests
	@echo   make test-unit  - Run C++ unit tests
	@echo   make test-api   - Run API endpoint integration tests
	@echo   make db         - Start PostgreSQL database via Docker
	@echo   make stop-db    - Stop PostgreSQL database
	@echo   make migrate      - Run pending database migrations (alias to migrate-up)
	@echo   make migrate-up   - Apply all pending *.up.sql migrations
	@echo   make migrate-down - Revert the latest applied migration (*.down.sql)
	@echo   make clean        - Clean build artifacts
	@echo =========================================================

db:
	docker compose up -d

stop-db:
	docker compose down

migrate: migrate-up

migrate-up:
ifeq ($(OS),Windows_NT)
	@powershell -NoProfile -ExecutionPolicy Bypass -File scripts/migrate.ps1 -Direction up
else
	@bash scripts/migrate.sh up
endif

migrate-down:
ifeq ($(OS),Windows_NT)
	@powershell -NoProfile -ExecutionPolicy Bypass -File scripts/migrate.ps1 -Direction down
else
	@bash scripts/migrate.sh down
endif

build:
	$(CMAKE) $(CMAKE_FLAGS)
	$(CMAKE) --build $(BUILD_DIR) --config $(BUILD_TYPE)

run:
ifeq ($(OS),Windows_NT)
	@powershell -NoProfile -Command "if (Test-Path '$(BUILD_DIR)/$(BUILD_TYPE)/backend_server.exe') { & '$(BUILD_DIR)/$(BUILD_TYPE)/backend_server.exe' } elseif (Test-Path '$(BUILD_DIR)/backend_server.exe') { & '$(BUILD_DIR)/backend_server.exe' } else { Write-Host '[ERROR] Binary not found. Run make build first.' -ForegroundColor Red; exit 1 }"
else
	@if [ -f "$(BUILD_DIR)/$(BUILD_TYPE)/backend_server" ]; then \
		"$(BUILD_DIR)/$(BUILD_TYPE)/backend_server"; \
	elif [ -f "$(BUILD_DIR)/backend_server" ]; then \
		"$(BUILD_DIR)/backend_server"; \
	else \
		echo "[ERROR] Binary not found. Run make build first."; exit 1; \
	fi
endif

dev: build run

test-unit:
ifeq ($(OS),Windows_NT)
	@powershell -NoProfile -Command "if (Test-Path '$(BUILD_DIR)/$(BUILD_TYPE)/unit_tests.exe') { & '$(BUILD_DIR)/$(BUILD_TYPE)/unit_tests.exe' } elseif (Test-Path '$(BUILD_DIR)/unit_tests.exe') { & '$(BUILD_DIR)/unit_tests.exe' } else { Write-Host '[ERROR] Unit tests not found. Run make build first.' -ForegroundColor Red; exit 1 }"
else
	@if [ -f "$(BUILD_DIR)/$(BUILD_TYPE)/unit_tests" ]; then \
		"$(BUILD_DIR)/$(BUILD_TYPE)/unit_tests"; \
	elif [ -f "$(BUILD_DIR)/unit_tests" ]; then \
		"$(BUILD_DIR)/unit_tests"; \
	else \
		echo "[ERROR] Unit tests not found. Run make build first."; exit 1; \
	fi
endif

test-api:
	powershell -ExecutionPolicy Bypass -File scripts/test-api.ps1

test: test-unit test-api

ifeq ($(OS),Windows_NT)
clean:
	@powershell -NoProfile -Command "if (Test-Path '$(BUILD_DIR)') { Remove-Item -Recurse -Force '$(BUILD_DIR)' }"
else
clean:
	@rm -rf $(BUILD_DIR)
endif
