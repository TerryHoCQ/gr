#!/bin/bash

GERSEMI_VERSION="0.27.5"
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
VENV_NAME=".gersemi_venv"


install_gersemi () {
    if [[ ! -d "${VENV_NAME}" ]]; then
        python3 -m venv "${VENV_NAME}" || return
    fi
    # Guarantee that all team members use the same version (of dependent packages)
    "${VENV_NAME}/bin/pip" -qqq install --upgrade --upgrade-strategy eager "gersemi==${GERSEMI_VERSION}" && \
    echo "$(cd "${VENV_NAME}" && pwd)/bin/gersemi"
}


main () {
    cd "${SCRIPT_DIR}" || return
    # Always use a pinned version of `gersemi`
    install_gersemi
}

main "$@"
