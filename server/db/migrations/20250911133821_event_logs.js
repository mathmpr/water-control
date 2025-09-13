exports.up = function(knex) {
    return knex.schema.createTable('event_logs', function(table) {
        table.increments('id').primary();
        table.integer('user_id');
        table.boolean('value');
        table.string('trigger_type', 50);
        table.integer('income');
        table.integer('detect');
        table.timestamps(true, true);
        table.foreign('user_id').references('id').inTable('users').onDelete('CASCADE');
    });
};

exports.down = function(knex) {
    return knex.schema.dropTableIfExists('event_logs');
};
