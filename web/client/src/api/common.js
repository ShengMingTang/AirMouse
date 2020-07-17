import axios from 'axios';
import { off } from 'process';
const qs = require('querystring');

const config = {
    headers: {
        'Content-Type': 'application/x-www-form-urlencoded'
    }
};


export const BLOCK_SIZE = 5;

// #tested base64 <-> blob
function base64ToArrayBuffer(base64){
    var binaryString = window.atob(base64); // Comment this if not using base64
    var bytes = new Uint8Array(binaryString.length);
    return bytes.map((byte, i) => binaryString.charCodeAt(i));
}
export function bin2Base64(bin){
    var data = btoa(bin);
    data = data.replace(/=/g, '_');
    data = data.replace(/\+/g, '#');
    data = data.replace(/\//g, '^');
    return data;
}
function base64PostFilter(base){
    var data = base;
    data = data.replace(/_/g, '=');
    data = data.replace(/#/g, '+');
    data = data.replace(/^/g, '/');
    return data;
}

// #tested
function createXHR(method, url, onload){
    var xhr = new XMLHttpRequest();
    xhr.open(method, url);
    xhr.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');
    xhr.onload = onload;
    return xhr;
}

// currently we support one thread for push, one thread for pull
export function pushStartToken(filename){
    return `__SL_P_UZ0=${filename}`;
}
export function pushEndToken(filename){
    return `__SL_P_UZ1=${filename}`;
}
export function pullStartToken(filename){
    return `__SL_P_UZ2=${filename}`;
}
export function pullEndToken(filename){
    return `__SL_P_UZ3=${filename}`;
}

export function postTest(){
    return axios.post('/', qs.stringify({
        __SL_P_ZZ0: '1^2',
        __SL_P_ZZ1: '1#2',
        __SL_P_ZZ2: '1_2',
    }), config).then(function(res){
        if(res.status < 200 || res.status > 299){
            throw new Error(`Unexpected Error ${res.status}`);
        }
        return res.status;
    })
}

export function pushStart(filename){
    return axios.post('/', qs.stringify({
        __SL_P_UZ0: filename
    }), config).then(function(res){
        if(res.status < 200 || res.status > 299){
            throw new Error(`Unexpected Error ${res.status}`);
        }
        return res.status;
    });
}
export function pushMore(file64, seq, offset){
    const data = `__SL_P_U${seq.toString().padStart(2, '0')}`;

    return axios.post('/', qs.stringify({
        [data]: file64.slice(offset, offset + BLOCK_SIZE),
    }), config).then(function(res){
        if(res.status < 200 || res.status > 299){
            throw new Error(`Unexpected Error ${res.status}`);
        }
        return res.status;
    });
}
export function pushEnd(filename){
    return axios.post('/', qs.stringify({
        __SL_P_UZ1: filename
    }), config).then(function(res){
        if(res.status < 200 || res.status > 299){
            throw new Error(`Unexpected Error ${res.status}`);
        }
        return res.status;
    });
}
