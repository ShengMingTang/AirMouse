const path = require('path');
const webpack = require('webpack');

const srcPath = path.resolve(__dirname, 'src');
const distPath = path.resolve(__dirname, 'dist');

// minimize code size
const TerserPlugin = require('terser-webpack-plugin');

module.exports = {
    // mode: 'development',
    mode: 'production',
    context: srcPath,
    resolve: {
        alias: {
            states: path.resolve(srcPath, 'states'),
            utilities: path.resolve(srcPath, 'utilities'),
            components: path.resolve(srcPath, 'components'),
            api: path.resolve(srcPath, 'api')
        }
    },
    entry: {
        index: './index.jsx',
        vendor: ['react', 'react-dom']
        // index: {import: './src/index.jsx', dependOn: 'shared'},
        // vendor: {import: './src/'}
    },
    output: {
        path: distPath,
        filename: '[name].js'
    },
    module: {
        rules: [
            {
                test: /\.(js|jsx)$/,
                exclude: /(node_modules)/,
                use: [
                    {
                        loader: 'babel-loader',
                        options: {
                            presets: [
                                [
                                    '@babel/preset-env',{
                                        modules: false
                                    }
                                ],
                                '@babel/preset-react'
                            ],
                            plugins: [
                                '@babel/plugin-proposal-class-properties',
                                '@babel/plugin-proposal-object-rest-spread',
                            ]
                        }
                    }
                ]
            },
            {
                test: /.css$/,
                use: [
                    'style-loader',
                    {
                        loader: 'css-loader',
                        options: {
                            url: false
                        }
                    }
                ]
                // loader: "style-loader!css-loader",
            }
        ]
    },
    optimization: {
        splitChunks: {
            cacheGroups: {
                vendor: {          
                    minChunks: 2,
                    name: 'vendor',
                    chunks: 'all'
                }
            }
        },
        minimizer: [new TerserPlugin({ /* additional options here */ })],
    },
    devtool: 'source-map'
}