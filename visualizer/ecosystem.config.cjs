module.exports = {
  apps: [
    {
      name: 'potatotyper-site',
      script: './node_modules/serve/build/main.js',
      args: '-s dist -l tcp://0.0.0.0:4173',
      cwd: __dirname,
      env: {
        NODE_ENV: 'production',
      },
    },
  ],
}
