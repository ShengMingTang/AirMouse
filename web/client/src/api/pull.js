// #not-tested for pull data
function createXMLXHR(url, onload){
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url);
    xhr.responseType = 'document';
    xhr.overrideMimeType('text/xml');
    xhr.onload = onload;
    return xhr;
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
function serialPull(filename){
    var container = '';
    // tell cc3200 we are going to fetch file
    var startXhr = createXHR('POST', '/', function(e){
        // start pulling
        createAndSendPullXhr(container, '/pull.html', filename);
    });

    startXhr.send(pullStartToken(filename));
}
