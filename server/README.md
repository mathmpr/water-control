# Monólito em Node

A escolha das libs utilizadas nesse monólito foi feita com base na experiência de mercado e na maturidade das mesmas. O objetivo é fornecer uma base sólida para o desenvolvimento de aplicações Node.js.
A escolha também levou em conta e se inspirou em tecnologias que conheco e que funcionam muito bem.

- Objection + Knex: ORM e query builder para Node.js, que oferece uma interface simples e poderosa para interagir com bancos de dados SQL. Se assemelha a migrations e eloquente do Yii 2.
- Express: Framework minimalista para Node.js, que facilita a criação de APIs e aplicações web.
- Passport: Middleware de autenticação para Node.js, que suporta diversas estratégias de autenticação, como OAuth, JWT e sessões.
- Dotenv: Biblioteca para carregar variáveis de ambiente a partir de um arquivo `.env`, facilitando a configuração da aplicação.
- Sqlite3: Driver para SQLite, um banco de dados leve e fácil de usar, ideal para desenvolvimento e testes.

## Instalação

```bash
npm install
```

## Configuração
Crie um arquivo `.env` na raiz do projeto e configure as variáveis de ambiente necessárias. Um exemplo de arquivo `.env` pode ser encontrado em `.env.example`.

```bash
cp .env.example .env
```

## Execução
Para iniciar o servidor, execute o seguinte comando:

```bash
npm start
```