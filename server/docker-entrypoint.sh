#!/bin/sh
set -e

npx knex migrate:latest
npx knex seed:run

exec "$@"
