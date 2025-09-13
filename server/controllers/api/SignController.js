const User = require('../../models/User');
const jwt = require('jsonwebtoken');
const bcrypt = require('bcrypt');

const setToken = async (user, res) => {
    const token = jwt.sign({ id: user.id }, process.env.JWT_SECRET, {
        expiresIn: '1d',
    });
    user.token = token;
    await User.query().patchAndFetchById(user.id, {
        token: token
    });
    res.cookie('token', token, { httpOnly: true, secure: true });
    res.json({ token, user: { id: user.id, email: user.email } });
}

module.exports = {
    signIn: async (req, res) => {
        const { email, password } = req.body;
        const user = await User.query()
            .where('email', email)
            .orWhere('username', email)
            .first();
        if (!user || !(await user.verifyPassword(password))) {
            return res.status(401).json({ error: 'invalid credentials' });
        }
        await setToken(user, res);
    },
    signOut: (req, res) => {
        req.user = null;
        res.clearCookie('token');
        res.json({ success: true, message: 'signed out successfully' });
    }
};