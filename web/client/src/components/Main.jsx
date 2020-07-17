import React from 'react';
import PropTypes from 'prop-types';
import {connect} from 'react-redux';

import {Button} from 'reactstrap';

import FileSelector from 'components/FileSelector.jsx';

import {test as testFromAct} from 'states/main-actions.js';

class Main extends React.Component {
    static propTypes = {
        store: PropTypes.object,
        dispatch: PropTypes.func,
    };

    constructor(props){
        super(props);
        this.test = this.test.bind(this);
    }
    render(){
        return (
            <div>
                <div>
                    <p>P2P device name    : __SL_G_R.A</p>
                    <p>P2P device type    : __SL_G_R.B</p>
                    <p>IP address as STA  : __SL_G_N.A</p>
                    <p>IP address as AP   : __SL_G_N.P</p>
                    <p>MAC address        : __SL_G_N.D</p>
                    <p>Token test         : __SL_G_UXX</p>
                </div>
                <Button onClick={this.test}>post test</Button>
                <p>{this.props.status}</p>
                <FileSelector/>
                <div id='downloaders'>
                </div>
            </div>
        );
    }
    test(){
        this.props.dispatch(testFromAct());
    }
};

export default connect(state => ({
    ...state.main
}))(Main);