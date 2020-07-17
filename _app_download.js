function base64ToArrayBuffer(base64){
    var binaryString = window.atob(base64); // Comment this if not using base64
    var bytes = new Uint8Array(binaryString.length);
    return bytes.map((byte, i) => binaryString.charCodeAt(i));
}

function createLinkForBase64Str(base64Str, filename){
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