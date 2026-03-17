const fs = require('fs').promises;
const { minify } = require('html-minifier');
const CleanCSS = require('clean-css');
const Terser = require('terser');
const path = require('path');
const { mkdirp } = require('mkdirp');

async function build() {
    const webDir = './web';
    const dataDir = './data';
    const outputHtmlPath = path.join(dataDir, 'index.html');

    try {
        console.log('Building and minifying files...');

        let htmlContent = await fs.readFile(path.join(webDir, 'index.html'), 'utf8');
        console.log('✓ HTML read');

        const cssContent = await fs.readFile(path.join(webDir, 'css', 'kprox.css'), 'utf8');
        const minifiedCss = new CleanCSS({
            level: 2,
            returnPromise: false
        }).minify(cssContent).styles;
        console.log('✓ CSS minified');

        const jsContent = await fs.readFile(path.join(webDir, 'js', 'kprox.js'), 'utf8');
        const terserResult = await Terser.minify(jsContent, {
            compress: {
                drop_console: false,
                drop_debugger: true,
                pure_funcs: [],
                unsafe: false
            },
            mangle: {
                reserved: []
            },
            format: {
                comments: false
            }
        });

        if (terserResult.error) {
            throw new Error(`Terser minification failed: ${terserResult.error}`);
        }
        const minifiedJs = terserResult.code;
        console.log('✓ JavaScript minified with Terser');

        htmlContent = htmlContent.replace(
            '<link rel="stylesheet" href="css/kprox.css">',
            `<style>${minifiedCss}</style>`
        );
        console.log('✓ CSS inlined');

        // Remove the external JS script tag — JS is inlined below
        htmlContent = htmlContent.replace(
            '<script src="js/kprox.js"></script>',
            ''
        );

        htmlContent = htmlContent.replace(
            '</body>',
            `<script>${minifiedJs}</script>\n</body>`
        );
        console.log('✓ JavaScript inlined');

        const minifiedHtml = minify(htmlContent, {
            collapseWhitespace: true,
            removeComments: true,
            removeRedundantAttributes: true,
            removeScriptTypeAttributes: true,
            removeStyleLinkTypeAttributes: true,
            useShortDoctype: true,
            minifyCSS: true,
        });
        console.log('✓ HTML minified');

        await mkdirp(dataDir);
        await fs.writeFile(outputHtmlPath, minifiedHtml);

        // Copy kprox.png to data/ if it exists
        const pngSrc = path.join(webDir, 'kprox.png');
        const pngDst = path.join(dataDir, 'kprox.png');
        try {
            await fs.copyFile(pngSrc, pngDst);
            console.log('✓ kprox.png copied to data/');
        } catch {
            console.log('ℹ kprox.png not found in web/, skipping');
        }

        // Copy TOKEN_REFERENCE.md to data/ so it is served via /api/docs
        try {
            await fs.copyFile('TOKEN_REFERENCE.md', path.join(dataDir, 'TOKEN_REFERENCE.md'));
            console.log('✓ TOKEN_REFERENCE.md copied to data/');
        } catch {
            console.log('ℹ TOKEN_REFERENCE.md not found, skipping');
        }

        // Copy API_REFERENCE.md to data/ so it is served via /api/apiref
        try {
            await fs.copyFile('API_REFERENCE.md', path.join(dataDir, 'API_REFERENCE.md'));
            console.log('✓ API_REFERENCE.md copied to data/');
        } catch {
            console.log('ℹ API_REFERENCE.md not found, skipping');
        }

        const originalHtmlSize = (await fs.stat(path.join(webDir, 'index.html'))).size;
        const originalCssSize  = (await fs.stat(path.join(webDir, 'css', 'kprox.css'))).size;
        const originalJsSize   = (await fs.stat(path.join(webDir, 'js', 'kprox.js'))).size;
        const finalSize        = (await fs.stat(outputHtmlPath)).size;
        const originalTotalSize = originalHtmlSize + originalCssSize + originalJsSize;
        const compressionRatio  = ((originalTotalSize - finalSize) / originalTotalSize * 100).toFixed(1);

        console.log('\n📊 Minification Results:');
        console.log(`Original total size: ${originalTotalSize} bytes`);
        console.log(`Final size:          ${finalSize} bytes`);
        console.log(`Compression:         ${compressionRatio}% smaller`);
        console.log(`\n✅ Successfully compiled to ${outputHtmlPath}`);

    } catch (error) {
        console.error('❌ Error during compilation:', error);
        process.exit(1);
    }
}

if (require.main === module) {
    build();
}

module.exports = { build };
