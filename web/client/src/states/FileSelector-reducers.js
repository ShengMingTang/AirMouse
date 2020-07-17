const initFileSelectorState = {
    filename: 'Not uploaded yet',
    file64 : '',
    status: 'Okay',

    uploading: false,
    seq: 0,
    offset: 0,
};

export function fileSelector(state = initFileSelectorState, action){
    switch(action.type){
        case '@FILESELECTOR/ONCHANGE':
            return {
                ...state
            };
        case '@FILESELECTOR/ONLOAD':
            return {
                ...state,
                file64: action.file64,
                filename: action.filename
            }
        case '@FILESELECTOR/PUSH_START':
            return {
                ...state,
                status: action.status,
                uploading: true,
                seq: action.seq,
                offset: action.offset
            }
        case '@FILESELECTOR/PUSH_PROGRESS':
            return {
                ...state,
                status: action.status,
                seq: action.seq,
                offset: action.offset
            }
        case '@FILESELECTOR/PUSH_END':
            return {
                ...state,
                uploading: false,
                status: action.status
            }
        default:
            return state;
    }
};