#!/bin/bash

# Supposed to be executed at root of repository from Travis CI

set -e
cd ./next/compiler
make build
go test -v ./...
