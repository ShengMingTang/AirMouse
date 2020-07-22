// const express = require('express');
// const app = express();

// app.use('/', function(req, res, next){
//     console.log('Request Type:', req.method);
//     next();
// })

// app.listen(3000, () => {
//     console.log('listen on 3000');
// })

const express = require('express');

const app = express();

// app.use(requestLogger); // debug only
app.use(express.static('../client/dist', {
    setHeaders: (res, path, stat) => {
        res.set('Cache-Control', 'public, s-maxage=86400');
    }
}));
app.get('/*', (req, res) => res.redirect('/'));

const port = 3000;
app.listen(port, () => {
    console.log(`Server is up and running on port ${port}...`);
});
