const jwt = require('jsonwebtoken');
const User = require('../models/User');

const signedMiddleware = async (req, res, next) => {
    const authHeader = req.headers.authorization;
    const token = req.cookies.token ?? authHeader?.split(' ')[1];

    let user;
    if (token) {
        try {
            const decoded = jwt.verify(token, process.env.JWT_SECRET);
            user = await User.query().findById(decoded.id);
        } catch (err) {}
    }
    if (user) {
        return res.redirect('/admin');
    }
    next();
};

module.exports = signedMiddleware;
