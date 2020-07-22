const initMainState = {
    status: '',
    document: null,
    test: '1'
};

export function main(state = initMainState, action){
    switch(action.type){
        case '@MAIN/STATUS':
            return {
                ...state,
                status: action.status
            };
        case '@MAIN/DOC':
            return {
                ...state,
                document: action.document
            }
        default:
            return state;
    }
}