import React from 'react';
import ReactDom from 'react-dom';
import {createStore, combineReducers, compose, applyMiddleware} from 'redux';
import thunkMiddleware from 'redux-thunk';
// import loggerMiddleware from 'redux-logger';
import {Provider} from 'react-redux';



import Main from 'components/Main.jsx';

// import some reducers
import {main} from 'states/main-reducers.js';
import {fileSelector} from 'states/fileSelector-reducers.js';

import 'bootstrap/dist/css/bootstrap.css';

window.onload = function(){
    const composeEnhancers = window.__REDUX_DEVTOOLS_EXTENSION_COMPOSE__ || compose;
    const store = createStore(combineReducers({
        main, fileSelector
    }), composeEnhancers(applyMiddleware(thunkMiddleware/*, loggerMiddleware*/)));
    ReactDom.render(
        <Provider store={store}>
            <Main/>
        </Provider>,
        document.getElementById('root')
    );
    // submitBtn.onclick = function(reader, fileInput, e){
    //     serialPush(reader, fileInput.files[0].name);
    // }.bind(null, reader, fileInput);
}