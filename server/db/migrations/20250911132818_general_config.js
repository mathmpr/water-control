exports.up = function(knex) {
    return knex.schema.createTable('general_config', function(table) {
        table.increments('id').primary();
        table.string('config').notNullable();
        table.string('value').notNullable().unique();
        table.timestamps(true, true);
    });
};

exports.down = function(knex) {
    return knex.schema.dropTableIfExists('general_config');
};
