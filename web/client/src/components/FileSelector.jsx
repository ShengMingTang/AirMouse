import React from 'react';
import PropTypes from 'prop-types';
import {connect} from 'react-redux';

import {Text, Button} from 'reactstrap';

import {onChange, onPush} from 'states/fileSelector-actions.js';

class FileSelector extends React.Component{
    static propTypes = {
        store: PropTypes.object,
        dispatch: PropTypes.func,
    };

    constructor(props){
        super(props);
        this.handleChange = this.handleChange.bind(this);
        this.handlePush = this.handlePush.bind(this);
    }
    render(){
        return (
            <div>
                <input type="file" onChange= {(e) => {
                    this.handleChange(e.target.files)
                }}/>
                <p>
                    {this.props.filename}
                </p>
                <p>
                    {this.props.file64}
                </p>

                <Button onClick={this.handlePush}>Push</Button>
                <p>{this.props.status}</p>
                <p>{this.props.seq}:{this.props.offset}/{this.props.file64.length}</p>
            </div>
        );
    }
    handleChange(selectorFiles){
        this.props.dispatch(onChange(selectorFiles));
    }
    handlePush(){
        this.props.dispatch(onPush(this.props.file64, this.props.filename));
    }
}

export default connect(state => ({
    ...state.fileSelector
}))(FileSelector);