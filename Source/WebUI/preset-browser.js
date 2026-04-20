// Full preset browser modal (search, columns, sidebar, favorites)

let allPresetsCache = null;

function initPresetBrowser() {
    // Populate browser on first open
    loadPresetsCache();
}

function loadPresetsCache() {
    if (window.Juce && window.Juce.getAllPresets) {
        const json = window.Juce.getAllPresets();
        allPresetsCache = JSON.parse(json);
    }
}

function populatePresetBrowser() {
    if (!allPresetsCache) {
        loadPresetsCache();
    }

    const modal = document.getElementById('preset-browser-modal');
    const html = `
        <div style="display: flex; width: 100%; height: 100%; background: #1a1a2e;">
            <!-- Left Sidebar -->
            <div style="
                width: 140px;
                background: #0f3460;
                border-right: 1px solid #16384d;
                overflow-y: auto;
                padding: 8px;
            ">
                <div style="color: #999; font-size: 9px; text-transform: uppercase; margin-bottom: 8px;">Categories</div>
                <div style="padding: 6px 8px; cursor: pointer; color: #16c784; border-left: 2px solid #16c784;" onclick="filterPresetsByCategory('all')">
                    Explore
                </div>
                <div style="padding: 6px 8px; cursor: pointer; color: #999; border-left: 2px solid transparent;" onclick="filterPresetsByCategory('factory')">
                    Factory
                </div>
                <div style="padding: 6px 8px; cursor: pointer; color: #999; border-left: 2px solid transparent;" onclick="filterPresetsByCategory('user')">
                    User
                </div>
            </div>

            <!-- Main Content -->
            <div style="flex: 1; display: flex; flex-direction: column; border-right: 1px solid #16384d;">
                <!-- Search Bar -->
                <div style="padding: 8px; border-bottom: 1px solid #16384d; display: flex; gap: 6px; align-items: center;">
                    <input id="preset-search-input"
                           type="text"
                           placeholder="Search presets..."
                           style="
                               flex: 1;
                               background: #16384d;
                               color: #16c784;
                               border: 1px solid #16384d;
                               padding: 4px 6px;
                               border-radius: 3px;
                               font-size: 10px;
                           "
                           onkeyup="filterPresetsOnSearch()">
                    <button onclick="clearPresetSearch()" style="
                        background: none;
                        border: none;
                        color: #999;
                        cursor: pointer;
                        font-size: 10px;
                        padding: 2px 4px;
                    ">✕ Clear</button>
                </div>

                <!-- Preset List -->
                <div style="flex: 1; overflow-y: auto; padding: 8px;">
                    <div id="preset-list" style="display: grid; gap: 6px;">
                        <!-- Populated by JavaScript -->
                    </div>
                </div>
            </div>

            <!-- Right Preview Panel -->
            <div style="
                width: 140px;
                background: #0f3460;
                border-left: 1px solid #16384d;
                padding: 8px;
                display: flex;
                flex-direction: column;
            ">
                <div style="color: #999; font-size: 9px; text-transform: uppercase; margin-bottom: 6px;">Preview</div>
                <div id="preset-preview" style="
                    background: #16384d;
                    flex: 1;
                    border-radius: 3px;
                    display: flex;
                    align-items: center;
                    justify-content: center;
                    font-size: 9px;
                    color: #666;
                    text-align: center;
                    padding: 8px;
                ">
                    Select a preset
                </div>
            </div>
        </div>
    `;

    modal.innerHTML = html;
    renderPresetList(allPresetsCache);

    document.getElementById('preset-search-input').focus();
}

function renderPresetList(presets) {
    const listDiv = document.getElementById('preset-list');
    let html = '';

    // Flatten and render presets
    for (const [packName, presetArray] of Object.entries(presets)) {
        for (const preset of presetArray) {
            const bgColor = preset.metadata.name === currentPresetName ? '#16384d' : '#0f3460';
            const heartIcon = preset.metadata.isFavorite ? '♥' : '♡';
            const heartColor = preset.metadata.isFavorite ? '#e94560' : '#999';

            html += `
                <div style="
                    display: grid;
                    grid-template-columns: 1fr 80px 100px 20px;
                    gap: 8px;
                    padding: 6px;
                    background: ${bgColor};
                    border-radius: 3px;
                    align-items: center;
                    cursor: pointer;
                    font-size: 10px;
                "
                onmouseover="this.style.background='#16384d'"
                onmouseout="this.style.background='${bgColor}'"
                onclick="loadPresetFromBrowser('${preset.metadata.name}', '${packName}')">
                    <div style="color: #16c784;">${preset.metadata.name}</div>
                    <div style="color: #999;">${preset.metadata.type}</div>
                    <div style="color: #999;">${preset.metadata.designer}</div>
                    <div style="color: ${heartColor}; cursor: pointer; text-align: center;" onclick="event.stopPropagation(); toggleFavoriteFromBrowser('${preset.metadata.name}', '${packName}')">${heartIcon}</div>
                </div>
            `;
        }
    }

    listDiv.innerHTML = html || '<div style="color: #666; padding: 16px;">No presets found</div>';
}

function filterPresetsOnSearch() {
    const query = document.getElementById('preset-search-input').value.toLowerCase();

    const filtered = {};
    for (const [packName, presetArray] of Object.entries(allPresetsCache)) {
        filtered[packName] = presetArray.filter(p =>
            p.metadata.name.toLowerCase().includes(query)
        );
    }

    renderPresetList(filtered);
}

function clearPresetSearch() {
    document.getElementById('preset-search-input').value = '';
    renderPresetList(allPresetsCache);
}

function filterPresetsByCategory(category) {
    if (category === 'all') {
        renderPresetList(allPresetsCache);
    } else if (category === 'factory') {
        const filtered = { Factory: allPresetsCache.Factory || [] };
        renderPresetList(filtered);
    } else if (category === 'user') {
        const filtered = { User: allPresetsCache.User || [] };
        renderPresetList(filtered);
    }
}

function loadPresetFromBrowser(presetName, packName) {
    if (window.Juce && window.Juce.loadPreset) {
        window.Juce.loadPreset(presetName, packName);
        setCurrentPreset(presetName, false, packName);
        hidePresetBrowser();
    }
}

function toggleFavoriteFromBrowser(presetName, packName) {
    // TODO: call native function to toggle favorite
    // Update cache and re-render
    if (allPresetsCache[packName]) {
        const preset = allPresetsCache[packName].find(p => p.metadata.name === presetName);
        if (preset) {
            preset.metadata.isFavorite = !preset.metadata.isFavorite;
            renderPresetList(allPresetsCache);
        }
    }
}

// Initialize on page load
document.addEventListener('DOMContentLoaded', initPresetBrowser);
