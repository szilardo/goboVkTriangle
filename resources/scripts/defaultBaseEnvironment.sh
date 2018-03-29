#!/bin/bash

export BASE_ENVIRONMENT_SCRIPT_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )";
export BUILD_ROOT="${BASE_ENVIRONMENT_SCRIPT_PATH}/../../../build/";
export INSTALL_PREFIX="${BASE_ENVIRONMENT_SCRIPT_PATH}/../../../sysroot/";
export PROJECT_ROOT="${BASE_ENVIRONMENT_SCRIPT_PATH}/../../../";
