
import axios from 'axios';
import {BLOCK_SIZE, bin2Base64} from './common.js';

//****************************************************************************
// #tested
//! List the files in the given directory
//!
//! \param filename(str), seq(generator for next argument)
//!
//! \return None.
//
//****************************************************************************
function createAndSendPushXhr(filename, seq){
    var param = seq();
    if(param !== ''){
        var xhr = createXHR('POST', '/', createAndSendPushXhr.bind(null, filename, seq));
        xhr.send(param);
    }
    else{
        var xhr = createXHR('POST', '/');
        xhr.send(pushEndToken(filename));
    }
}

// #tested
//****************************************************************************
// #tested
//! gen tokens for push in [seqStart, seqEnd]
//!
//! \param data(base64Str), blockSize(int), seqStart(int), seqEnd(int)
//!
//! \yield token for push data
//
//****************************************************************************
function genPushSeqGen(data, blockSize, seqStart, seqEnd){
    let seq = 0;
    let seqSize = seqEnd - seqStart;
    return function(){
        // var temp = seq % 100;
        let temp = seqStart + (seq + 1) % seqSize;
        let transmit = data.slice(temp * blockSize, (temp + 1) * blockSize);
        seq = (seq + 1) % seqSize;

        if(transmit !== '') // more
            return `__SL_P_U${temp.toString().padStart(2, '0')}=${transmit}`;
        else // finished
            return '';
    }
}

// #tested
function serialPush(reader, filename){
    var data = bin2Base64(reader.result);
    var startXhr = createXHR('POST', '/', function(e){
        var seq = genPushSeqGen(data, BLOCK_SIZE);
        createAndSendPushXhr(filename, seq);
    });
    startXhr.send(pushStartToken(filename));
}

export function createLinkForBase64Str(base64Str, filename){
    var body = base64ToArrayBuffer(base64Str);
    var blob = new Blob([body]);
    var link = document.createElement('a');
    // Browsers that support HTML5 download attribute
    if (link.download !== undefined) {
        var url = URL.createObjectURL(blob);
        link.setAttribute('href', url);
        link.setAttribute('download', filename);
        link.innerText = filename;
        return link;
    }
    else{
        console.error('Browser does not support download attribute');
    }
}