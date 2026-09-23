# Node monolith

The choice of LIBs used in this monolith was made based on the market experience and their maturity. The goal is to provide a solid base for the development of applications node.js.
The choice also took into account and was inspired by technologies I know and work very well.

- Objectation + Knex: ORM and Query Builder for Node.js, which offers a simple and powerful interface to interact with SQL databases. It resembles the migrations and eloquent of YII 2.
- Express: Minimalist Framework for Node.JS, which facilitates the creation of APIs and web applications.
- Passport: Authentication Middleware for Node.js, which supports various authentication strategies such as Oauth, JWT and sessions.
- Dotenv: Library to load environment variables from an `.env` file, facilitating the application configuration.
- MySQL 8 via mysql2. SQLite remains available for tests and migration of existing installations.

## Installation

```bash
npm install
```

## Configuração
Create a `.env` file at the project root and set the necessary environment variables. An example of a `.env` file can be found in` .env.example`.

```bash
cp .env-example .env
```

## Execução
To start the server, run the following command:

```bash
npm run init-db
npm start
```

## Docker

Configure an existing MySQL 8 server, with a dedicated database/user. Set
`DB_CLIENT=mysql2`, `DB_HOST`, `DB_PORT`, `DB_DATABASE`, `DB_USERNAME` and
`DB_PASSWORD` in the server environment. The example file documents these values.
Inside Docker, DB_HOST must point to a host reachable from the container;
127.0.0.1 would refer to the container itself. For Compose, set the variables
in the project-root `.env` or export them in the shell.

From the project root, build and start the server:

```bash
JWT_SECRET=change-me SECRET_KEY=change-me docker compose up -d --build
```

The container exposes:

- `1883`: MQTT TCP broker.
- `1888`: HTTP app and MQTT WebSocket broker.

Compose starts only the application; MySQL runs externally. On startup,
the container runs migrations and idempotent seeds before starting `main.js`.
Existing deployments with DATABASE_PATH and no DB_CLIENT keep using SQLite
until their data is transferred. DB_CLIENT=mysql2 always selects MySQL.
Do not remove the existing data volume before completing the migration.

`npm test` uses isolated SQLite memory storage. For MySQL testing, provide
TEST_MYSQL=1 and the DB_* variables for a dedicated, empty database whose
name starts with verify_. The suite refuses an existing populated database.
CI verifies both engines before publishing the image.
