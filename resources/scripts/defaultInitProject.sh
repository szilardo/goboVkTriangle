#!/bin/bash
SCRIPT_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )";
. "${SCRIPT_PATH}/defaultBaseEnvironment.sh";
mkdir -p "${BUILD_ROOT}";
mkdir -p "${INSTALL_PREFIX}";
cp "${PROJECT_ROOT}/goboVkTriangle/resources/configs/.clang-format"  "${PROJECT_ROOT}/goboVkTriangle";
cp "${PROJECT_ROOT}/goboVkTriangle/resources/configs/.gitignore"  "${PROJECT_ROOT}/goboVkTriangle";
cp "${PROJECT_ROOT}/goboVkTriangle/resources/configs/.gitmessage"  "${PROJECT_ROOT}/goboVkTriangle";

pushd "${PROJECT_ROOT}/goboVkTriangle" &> /dev/null;
mkdir -p .git/hooks;
printf "[user]\n\tname = Szilard Orban\n\temail = devszilardo@gmail.com\n" > ./.git/config 
printf "[commit]\n\ttemplate = \"${PROJECT_ROOT}/goboVkTriangle/.gitmessage\"\n" >> ./.git/config 
printf "[diff]\n\talgorithm = minimal\n\tmnemonicprefix = true\n" >> ./.git/config 
printf "[core]\n\teditor = vim\n" >> ./.git/config 
git init;
git add .;
git commit -m "Basic project structure";
cp "${PROJECT_ROOT}/goboVkTriangle/resources/configs/pre-commit" "${PROJECT_ROOT}/goboVkTriangle/.git/hooks/";
popd &> /dev/null;
