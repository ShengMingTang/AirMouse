import React from 'react';

import axios from 'axios';
import { off } from 'process';
import { func, object } from 'prop-types';
import { parse } from 'path';
const qs = require('querystring');

const baseUrl = '/';
// const baseUrl = 'localhost:3000';

const postConfig = {
    headers: {
        'Content-Type': 'application/x-www-form-urlencoded'
    }
};
const getConfig = {
    responseType: 'document',
    // headers: {
    //     'Content-Type': 'application/x-www-form-urlencoded'
    // },
}


export const BLOCK_SIZE = 50;

// #tested base64 <-> blob
function base64ToArrayBuffer(base64){
    var binaryString = window.atob(base64); // Comment this if not using base64
    var bytes = new Uint8Array(binaryString.length);
    return bytes.map((byte, i) => binaryString.charCodeAt(i));
}
export function bin2Base64(data){
    data = btoa(data);
    data = data.replace(/=/g, '_');
    data = data.replace(/\+/g, '#');
    data = data.replace(/\//g, '^');
    return data;
}
export function base64ToBin(data){
    data = data.replace(/_/g, '=');
    data = data.replace(/#/g, '+');
    data = data.replace(/\^/g, '/');
    data = atob(data);
    return data;
}

export function fileReaderHelper(path, file){
    return new Promise((resolve, reject) => {
        let reader = new FileReader();
        reader.addEventListener('load', () => {
            let retPath = path.slice();
            let route = retPath.split('/');
            let filename = route[route.length - 1];
            let comp = filename.split('.');
            filename = `${comp[0].slice(0, 8)}.${comp[1].slice(0, 3)}`;
            route[route.length - 1] = filename;
            retPath = route.join('/');
            resolve({file64: bin2Base64(reader.result), path: retPath});
        });
        reader.readAsBinaryString(file);
    })
}
export function listCc3200Fs(){
    return getDOM('/fs.html').then(doc => {
        const fsStr = doc.querySelector('#fs').innerHTML;
        const tree = JSON.parse(fsStr);
        return cc3200FsToFileSys(tree);
    }).catch(err => {
        return cc3200FsToFileSys({l:'/', c: [{l:'temp', c:[{l:'a.txt'}]}  ]})
    });
}

function cc3200FsToFileSys(obj, parent = ''){
    /*
        [
            {
                label: '/',
                children: [
                    {
                        label: 'temp/',
                        children: [
                            {
                                label: 'a.txt',
                            },
                        ]
                    },
                    {
                        label: 'small.txt',
                    }
                ]
            }
        ]
    */
   return {
       label: obj.l,
       parent,
       children: ('c' in obj) ? obj.c.map( p => (
           cc3200FsToFileSys(p, parent + obj.l + (obj.l === '/' ? '' : '/'))) 
        ) : null
   };
}

// currently we support one thread for push, one thread for pull
function pushStartToken(filename){
    return `__SL_P_UZ0=${filename}`;
}
function pushEndToken(filename){
    return `__SL_P_UZ1=${filename}`;
}
function pullStartToken(filename){
    return `__SL_P_Uz0=${filename}`;
}
function pullEndToken(filename){
    return `__SL_P_Uz1=${filename}`;
}


export function postTest(){
    return axios.post('/', qs.stringify({
        __SL_P_ZZ0: '1^2',
        __SL_P_ZZ1: '1#2',
        __SL_P_ZZ2: '1_2',
    }), postConfig).then(function(res){
        if(res.status < 200 || res.status > 299){
            throw new Error(`Unexpected Error ${res.status}`);
        }
        return res.status;
    });
}
export function lookTest(){
    return getDOM('fs.html').then(doc => {
        return doc.querySelector('#fs').innerHTML;
    }).catch(err => {
        return err;
    });
    // return axios.get('/fs.html', qs.stringify({}), getConfig).then(function(res){
    //     if(res.status < 200 || res.status > 299){
    //         throw new Error(`Unexpected Error ${res.status}`);
    //     }
    //     // res should be raw html
    //     let parser = new DOMParser();
    //     let html = parser.parseFromString(res.data, 'text/html');
    //     return html.querySelector('#test').innerHTML;
    // }).catch(err => {
    //     return err;
    // });
}
function getDOM(url, params = {}){
    return axios.get(url,qs.stringify(params), getConfig).then(res => {
        if(res.status < 200 || res.status > 299){
            throw new Error(`Unexpected Error ${res.status}`);
        }
        let parser = new DOMParser();
        return parser.parseFromString(res.data, 'text/html');
    });
}

export function pushStart(filename){
    return axios.post('/', qs.stringify({
        __SL_P_UZ0: filename
    }), postConfig).then(function(res){
        if(res.status < 200 || res.status > 299){
            throw new Error(`Unexpected Error ${res.status}`);
        }
        return res.status;
    });
}
export function pushMore(file64, offset){
    // const data = `__SL_P_U${seq.toString().padStart(2, '0')}`;

    // return axios.post('/', qs.stringify({
    //     [data]: file64.slice(offset, offset + BLOCK_SIZE),
    // }), postConfig).then(function(res){
    //     if(res.status < 200 || res.status > 299){
    //         throw new Error(`Unexpected Error ${res.status}`);
    //     }
    //     return res.status;
    // });

    const batchSize = 100;
    let params = {};
    let i = 0;
    while( (i < batchSize) && (offset + BLOCK_SIZE * i < file64.length) ){
        params[`__SL_P_U${i.toString().padStart(2, '0')}`] = 
            file64.slice(offset + BLOCK_SIZE * i, offset + BLOCK_SIZE * (i + 1));
        i++;
    }
    return axios.post('/', qs.stringify(
        params
    ), postConfig).then((res) => {
        if(res.status < 200 || res.status > 299){
            throw new Error(`Unexpected Error ${res.status}`);
        }
        return {
            status: res.status,
            offsetNext: offset + BLOCK_SIZE * i
        };
    });

}
export function pushEnd(filename){
    return axios.post('/', qs.stringify({
        __SL_P_UZ1: filename
    }), postConfig).then(function(res){
        if(res.status < 200 || res.status > 299){
            throw new Error(`Unexpected Error ${res.status}`);
        }
        return res.status;
    });
}

export function pullStart(filename){
    return axios.post('/', qs.stringify({
        __SL_P_Uz0: filename
    }), postConfig).then(function(res){
        if(res.status < 200 || res.status > 299){
            throw new Error(`Unexpected Error ${res.status}`);
        }
        return res.status;
    });
}
export function pullMore(){
    // const data = `__SL_G_U${seq.toString().padStart(2, '0')}`;
    return axios.get('/info.html', getConfig).then(function(res){
        if(res.status < 200 || res.status > 299){
            throw new Error(`Unexpected Error ${res.status}`);
        }
        // res should be raw html
        let parser = new DOMParser();
        let html = parser.parse(res.data, 'text/html');
        return html;
    })
}
export function pullEnd(filename){
    return axios.post('/', qs.stringify({
        __SL_P_Uz1: filename
    }), postConfig).then(function(res){
        if(res.status < 200 || res.status > 299){
            throw new Error(`Unexpected Error ${res.status}`);
        }
        return res.status;
    });
}

export function createDownloader(file64, filename){
    const url = URL.createObjectURL( new Blob([base64ToBin(file64)]));
    return {
        url,
        filename
    };
}
export function downloadByUrl(url){ // add a element, trigger it and remove it
    const link = document.createElement('a');
    // Browsers that support HTML5 download attribute
    if (link.download !== undefined) {
        link.setAttribute('href', url);
        link.setAttribute('download', fileName);
        link.style.visibility = 'hidden';
        document.body.appendChild(link);
        link.click();
        document.body.removeChild(link);
    }
}
export function createAndDownloadBlobFile(body, filename) {
    const blob = new Blob([body]);
    const url = URL.createObjectURL(blob);

    if (navigator.msSaveBlob) {
      // IE 10+
      navigator.msSaveBlob(blob, fileName);
    } else {
      downloadByUrl(url);
    }
    return url;
  }

export function onSelectNode(node){
    if("items" in node){ // it is a directory
        // do nothing for now
    }
    else if(node.label === 'add file'){ // as add-file button

    }
    else{ // it is file
        let link = document.createElement('a');
        link.setAttribute('href', node.url);
        link.setAttribute('download', node.filename);
        link.style.visibility = 'hidden';
        document.body.appendChild(link);
        link.click();
        document.body.removeChild(link);
    }
}