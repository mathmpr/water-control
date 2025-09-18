const express = require('express');
const router = express.Router();

const SignController = require('../controllers/api/SignController');
const AlexaController = require('../controllers/api/AlexaController');


router.post('/sign-in', SignController.signIn);
router.get('/sign-out', SignController.signOut);

router.all('/alexa', AlexaController.index);

module.exports = router;