# Node monolith

The choice of LIBs used in this monolith was made based on the market experience and their maturity. The goal is to provide a solid base for the development of applications node.js.
The choice also took into account and was inspired by technologies I know and work very well.

- Objectation + Knex: ORM and Query Builder for Node.js, which offers a simple and powerful interface to interact with SQL databases. It resembles the migrations and eloquent of YII 2.
- Express: Minimalist Framework for Node.JS, which facilitates the creation of APIs and web applications.
- Passport: Authentication Middleware for Node.js, which supports various authentication strategies such as Oauth, JWT and sessions.
- Dotenv: Library to load environment variables from an `.env` file, facilitating the application configuration.
- SQLite3: SQLite Driver, a light and easy to use database, ideal for development and testing.

## Installation

```bash
npm install
```

## Configuração
Create a `.env` file at the project root and set the necessary environment variables. An example of a `.env` file can be found in` .env.example`.

```bash
cp .env.example .env
```

## Execução
To start the server, run the following command:

```bash
npm init-db
npm start
```

## Docker

From the project root, build and start the server with SQLite persisted in a Docker volume:

```bash
JWT_SECRET=change-me SECRET_KEY=change-me docker compose up -d --build
```

The container exposes:

- `1883`: MQTT TCP broker.
- `1888`: HTTP app and MQTT WebSocket broker.

SQLite is stored at `/data/dev.sqlite3` inside the container through the `water-control-data` volume. On startup, the container runs migrations and idempotent seeds before starting `main.js`.
