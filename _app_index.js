// #tested
function postTest(){
    var xhr = new XMLHttpRequest();
    var params = [];
    var paramDict = {};
    
    paramDict['__SL_P_ZZ0'] = 'Hello+123';
    paramDict['__SL_P_ZZ1'] = '1=2';
    paramDict['__SL_P_ZZ2'] = '1^2';
    paramDict['__SL_P_ZZ3'] = '1#2';
    paramDict['__SL_P_ZZ4'] = '1_2';

    xhr.open('POST', '/', true);
    xhr.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');

    for(var key in paramDict){
        params.push(`${key}=${paramDict[key]}`);
    }

    xhr.send(params.join('&'));
    console.log(params.join('&'));
}

// #tested
function pushStartToken(filename){
    return `__SL_P_UZ0=${filename}`;
}
// #tested
function pushEndToken(filename){
    return `__SL_P_UZ1=${filename}`;
}
// #tested
function pullStartToken(filename){
    return `__SL_P_UZ2=${filename}`;
}
// #tested
function pullEndToken(filename){
    return `__SL_P_UZ3=${filename}`;
}

// #tested
function bin2Base64(bin){
    var data = btoa(bin);
    data = data.replace(/=/g, '_');
    data = data.replace(/\+/g, '#');
    data = data.replace(/\//g, '^');
    return data;
}

function base64PostFilter(base){
    var data = base;
    data = data.replace(/^/g, '/');
    data = data.replace(/#/g, '+');
    data = data.replace(/_/g, '=');
    return data;
}

// #tested
function genPushSeqGen(data, blockSize){
    var seq = 0;
    return function(){
        var temp = seq % 100;
        var transmit = data.slice(temp * blockSize, (temp + 1) * blockSize);
        seq++;

        if(transmit !== '') // more
            return `__SL_P_U${(temp % 100).toString().padStart(2, '0')}=${transmit}`;
        else // finished
            return '';
    }
}

// #tested
function createXHR(method, url, onload){
    var xhr = new XMLHttpRequest();
    xhr.open(method, url);
    xhr.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');
    xhr.onload = onload;
    return xhr;
}


function createXMLXHR(url, onload){
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url);
    xhr.responseType = 'document';
    xhr.overrideMimeType('text/xml');
    xhr.onload = onload;
    return xhr;
}

// #tested
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

function createAndSendPullXhr(container, url, filename){
    var xhr;
    xhr = createXMLXHR(url, function(container, url, filename, e){
        var block = this.document.querySelector('#pullData').innerHTML;
        if(block !== ''){ // more
            container += block;
            createAndSendPullXhr(container, url, filename);
        }
        else{ // finished, tell cc3200 to close file
            var xxhr = createXHR('POST', '/', function(container, filename){
                addDownLoadLink(base64PostFilter(container), filename);
            }.bind(null, container, filename));

            xxhr.send(pullEndToken(filename));
        }
    }.bind(xhr, container, url, filename));
    
    xhr.send();
}

// #tested
function serialPush(reader, filename){
    var data = bin2Base64(reader.result);
    var startXhr = createXHR('POST', '/', function(e){
        var seq = genPushSeqGen(data, 5);
        createAndSendPushXhr(filename, seq);
    });
    startXhr.send(pushStartToken(filename));
}
function serialPull(filename){
    var container = '';
    // tell cc3200 we are going to fetch file
    var startXhr = createXHR('POST', '/', function(e){
        // start pulling
        createAndSendPullXhr(container, '/pull.html', filename);
    });

    startXhr.send(pullStartToken(filename));
}

// #tested
function addDownLoadLink(base64Str, filename){
    var container = document.querySelector('#downloaders');
    var link = createLinkForBase64Str(base64Str, filename);
    var p = document.createElement('p');
    p.appendChild(link);
    container.appendChild(p);
}
// #tested
function addListeners(reader, filename){
    reader.addEventListener('load', function(e){
        var base64Str = btoa(this.result);
        var original = atob(base64Str);
        console.log(`total = ${this.result.length}, encoded = ${base64Str.length}`);
        console.log(base64Str);
        // addDownLoadLink(base64Str, filename);
    }.bind(reader));

    reader.addEventListener('progress', function(e){
        console.log(e.loaded)
    });

}

window.onload = function(){
    var btn = document.querySelector('#test');
    btn.onclick = postTest;

    var fileInput = document.querySelector('#uploadFile');
    var submitBtn = document.querySelector('#submit');
    var reader = new FileReader();

    fileInput.addEventListener('change', function(reader, e){
        return function(){
            console.log(this);
            var selectedFile = this.files[0];
            if(selectedFile){
                addListeners(reader, this.files[0].name);
                reader.readAsBinaryString(selectedFile);
            }
        };
    }.bind(fileInput, reader)());

    submitBtn.onclick = function(reader, fileInput, e){
        serialPush(reader, fileInput.files[0].name);
    }.bind(null, reader, fileInput);
}