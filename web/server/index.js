// const server = require('http').createServer();
// const io = require('socket.io')(server);
// io.on('connection', client => {
//     console.log(client);
//     client.on('event', data => { console.log(data) });
//     client.on('disconnect', () => { /* … */ });
// });
// server.listen(8080);
const express = require('express');
const app = express();

app.use('/', function(req, res, next){
    console.log('Request Type:', req.method);
    next();
})

app.listen(3000, () => {
    console.log('listen on 3000');
})