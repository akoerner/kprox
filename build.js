const fs = require('fs').promises;
const zlib = require('zlib');
const { promisify } = require('util');
const gzip = promisify(zlib.gzip);
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

        // Embed markdown docs as a fetch interceptor prepended to the JS bundle.
        // The interceptor overrides window.fetch to return embedded content for
        // /api/docs, /api/apiref, /api/kpsref — intercepting both relative URLs
        // (when served from the device) and absolute URLs (endpoint + path).
        // This eliminates three HTTP requests and three SPIFFS files.
        let interceptorJs = '';
        const mdFiles = {
            '/api/docs':   'TOKEN_REFERENCE.md',
            '/api/apiref': 'API_REFERENCE.md',
            '/api/kpsref': 'KEYPROX_SCRIPT_REFERENCE.md',
        };
        const embeddedDocs = {};
        for (const [route, file] of Object.entries(mdFiles)) {
            try {
                const content = await fs.readFile(file, 'utf8');
                embeddedDocs[route] = content;
                console.log(`✓ Embedded ${file} (${content.length} bytes) → ${route}`);
            } catch {
                console.log(`ℹ ${file} not found, skipping`);
            }
        }
        if (Object.keys(embeddedDocs).length > 0) {
            const docsJson = JSON.stringify(embeddedDocs);
            interceptorJs = `(function(){var _d=${docsJson};var _o=window.fetch.bind(window);window.fetch=function(u,o){var s=String(u);for(var k in _d){if(s===k||s.endsWith(k))return Promise.resolve(new Response(_d[k],{status:200}));}return _o(u,o);};})();`;
        }

        htmlContent = htmlContent.replace(
            '</body>',
            `<script>${interceptorJs}${minifiedJs}</script>\n</body>`
        );
        console.log('✓ JavaScript inlined');

        let minifiedHtml = minify(htmlContent, {
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

        // Inline kprox.png as a base64 data URI if it is small enough.
        // Large PNGs (> 50KB) must be resized externally first — base64-encoding
        // a 277KB image adds ~370KB of ASCII that gzip cannot recover efficiently.
        // If the PNG is too large, it is copied to data/ and served separately
        // (the browser caches it after the first load via max-age=86400).
        const pngSrc = path.join(webDir, 'kprox.png');
        const pngDst = path.join(dataDir, 'kprox.png');
        const PNG_INLINE_LIMIT = 50 * 1024; // 50KB
        try {
            const pngBuf  = await fs.readFile(pngSrc);
            if (pngBuf.length <= PNG_INLINE_LIMIT) {
                const b64    = pngBuf.toString('base64');
                const dataUri = `data:image/png;base64,${b64}`;
                minifiedHtml = minifiedHtml.replace(
                    /src=["']kprox\.png["']/g,
                    `src="${dataUri}"`
                );
                console.log(`✓ kprox.png inlined as base64 (${pngBuf.length} bytes → no extra HTTP request)`);
            } else {
                await fs.copyFile(pngSrc, pngDst);
                console.log(`⚠ kprox.png (${pngBuf.length} bytes) exceeds ${PNG_INLINE_LIMIT} byte limit — copied to data/ instead.`);
                console.log('  Resize it to ≤200px wide before building to enable inlining.');
            }
        } catch {
            console.log('ℹ kprox.png not found in web/, skipping');
        }

        await fs.writeFile(outputHtmlPath, minifiedHtml);

        const gzipped = await gzip(Buffer.from(minifiedHtml, 'utf8'), { level: 9 });
        const outputGzPath = outputHtmlPath + '.gz';
        await fs.writeFile(outputGzPath, gzipped);
        console.log(`✓ Gzipped to ${outputGzPath} (${gzipped.length} bytes)`);

const gzSize            = (await fs.stat(outputGzPath)).size;
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
        console.log(`Gzip size:           ${gzSize} bytes (${((1 - gzSize/finalSize)*100).toFixed(1)}% smaller than minified)`);
        console.log(`\n✅ Successfully compiled to ${outputHtmlPath} + .gz`);

    } catch (error) {
        console.error('❌ Error during compilation:', error);
        process.exit(1);
    }
}

if (require.main === module) {
    build();
}

module.exports = { build };
