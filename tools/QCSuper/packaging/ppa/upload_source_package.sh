#!/bin/bash

# Print commands, exit on error
set -xe

# Go to script's directory
cd "$(dirname "$0")"

PKGNAME="qcsuper"
PKGVER="2.1.1"
PKGREL="1"

PPA_DIR="$(pwd)"
DEB_DIST_DIR="${PPA_DIR}/deb_dist"

rm -rf "${DEB_DIST_DIR}"

# Create a target directory for our new source package before we build it
temp_dir="$(mktemp -d)"

function cleanup_dirs {
    rm -rf "${temp_dir}"
}

trap cleanup_dirs INT TERM

# Needed to generate a .tar.gz that will contain a setup.py
# file generated from our pyproject.toml located at
# project root

poetry build -f sdist

DIST_FILE="${PPA_DIR}/../../dist/${PKGNAME}-${PKGVER}.tar.gz"

cd "${temp_dir}"

tar xvf "${DIST_FILE}"

find . -exec touch {} \;

rm -f "${DIST_FILE}"

tar zcvf "${DIST_FILE}" .

cd "${PPA_DIR}"

for version in noble questing resolute; do # bionic focal jammy noble questing

    py2dsc-deb -x stdeb.cfg --suite ${version} --sign-results --sign-key 87EC6DB535CC2A084B41E88EF675C22E1B4B2ACC \
        --debian-version ${PKGREL}${version} "${DIST_FILE}"

    dput ppa:marin-m/qcsuper "deb_dist/${PKGNAME}_${PKGVER}-${PKGREL}${version}_source.changes"

done

cleanup_dirs
