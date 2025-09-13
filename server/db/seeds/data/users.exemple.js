const bcrypt = require("bcrypt");

module.exports = [
    {
        id: 1,
        name: 'Exemple',
        email: 'exemple@gmail.com',
        username: 'exemple',
        password: bcrypt.hashSync('exemple', 10),
        roles: JSON.stringify(['admin', 'user']),
    },
]
