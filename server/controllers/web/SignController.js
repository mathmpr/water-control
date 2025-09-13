module.exports = {
    signIn: async (req, res) => {
        return res.render('signIn');
    },
    signUp: (req, res) => {
        return res.render('signUp');
    }
};