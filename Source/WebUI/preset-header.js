// Preset header controls and save modal

let currentPresetName = "Default";
let currentPresetIsFavorite = false;
let currentPresetIsModified = false;
let currentPresetPack = "Factory";

// Initialize header buttons
function initPresetHeader() {
    const browserBtn = document.getElementById('preset-browser-btn');
    const favoriteBtn = document.getElementById('preset-favorite-btn');
    const prevBtn = document.getElementById('preset-prev-btn');
    const nextBtn = document.getElementById('preset-next-btn');
    const saveBtn = document.getElementById('preset-save-btn');
    const nameDisplay = document.getElementById('preset-name');

    browserBtn.addEventListener('click', () => {
        const modal = document.getElementById('preset-browser-modal');
        if (modal.style.display === 'none') {
            showPresetBrowser();
        } else {
            hidePresetBrowser();
        }
    });

    favoriteBtn.addEventListener('click', () => {
        currentPresetIsFavorite = !currentPresetIsFavorite;
        updateFavoriteButton();
        // TODO: call native function to persist favorite (Phase 2)
    });

    prevBtn.addEventListener('click', () => {
        // TODO: Load previous preset in current category
    });

    nextBtn.addEventListener('click', () => {
        // TODO: Load next preset in current category
    });

    saveBtn.addEventListener('click', () => {
        showSaveModal();
    });

    // Listen to parameter changes (set modified flag)
    window.addEventListener('parameter-changed', () => {
        setPresetModified(true);
    });
}

function updateFavoriteButton() {
    const btn = document.getElementById('preset-favorite-btn');
    btn.textContent = currentPresetIsFavorite ? '♥' : '♡';
    btn.style.color = currentPresetIsFavorite ? '#e94560' : '#999';
}

function setPresetModified(isModified) {
    currentPresetIsModified = isModified;
    const indicator = document.getElementById('preset-modified-indicator');
    const saveBtn = document.getElementById('preset-save-btn');

    if (isModified) {
        indicator.style.display = 'inline';
        saveBtn.style.display = 'block';
    } else {
        indicator.style.display = 'none';
        saveBtn.style.display = 'none';
    }
}

function setCurrentPreset(name, isFavorite, packName) {
    currentPresetName = name;
    currentPresetIsFavorite = isFavorite;
    currentPresetPack = packName;
    setPresetModified(false);

    document.getElementById('preset-name').textContent = name;
    updateFavoriteButton();
}

function showSaveModal() {
    const modal = document.getElementById('preset-save-modal');
    const html = `
        <div style="
            background: #1a1a2e;
            border: 2px solid #0f3460;
            border-radius: 6px;
            padding: 16px;
            width: 100%;
            max-width: 320px;
            font-family: system-ui;
            font-size: 12px;
        ">
            <div style="color: #16c784; font-weight: 600; margin-bottom: 12px;">Save Preset</div>
            <div style="margin-bottom: 12px;">
                <label style="display: block; color: #999; font-size: 10px; text-transform: uppercase; margin-bottom: 4px;">
                    Preset Name
                </label>
                <input id="preset-name-input"
                       type="text"
                       value="${currentPresetName}"
                       style="
                           width: 100%;
                           background: #0f3460;
                           color: #16c784;
                           border: 1px solid #16384d;
                           padding: 6px 8px;
                           border-radius: 3px;
                           font-size: 12px;
                           box-sizing: border-box;
                       "
                       autofocus>
            </div>
            <div style="margin-bottom: 12px; font-size: 10px; color: #999;">
                <input id="preset-overwrite-checkbox" type="checkbox">
                <label for="preset-overwrite-checkbox">Overwrite existing preset</label>
            </div>
            <div style="display: flex; gap: 6px;">
                <button id="preset-save-confirm" style="
                    flex: 1;
                    background: #16c784;
                    color: #1a1a2e;
                    border: none;
                    padding: 6px;
                    border-radius: 3px;
                    cursor: pointer;
                    font-weight: 600;
                    font-size: 11px;
                ">Save</button>
                <button id="preset-save-cancel" style="
                    flex: 1;
                    background: #16384d;
                    color: #999;
                    border: 1px solid #16384d;
                    padding: 6px;
                    border-radius: 3px;
                    cursor: pointer;
                    font-size: 11px;
                ">Cancel</button>
            </div>
        </div>
    `;
    modal.innerHTML = html;
    modal.style.display = 'flex';

    document.getElementById('preset-save-confirm').addEventListener('click', () => {
        const newName = document.getElementById('preset-name-input').value;
        const shouldOverwrite = document.getElementById('preset-overwrite-checkbox').checked;

        // Call native function to save preset
        if (window.Juce && window.Juce.savePreset) {
            window.Juce.savePreset(newName, shouldOverwrite);
            setCurrentPreset(newName, false, 'User');
            modal.style.display = 'none';
        }
    });

    document.getElementById('preset-save-cancel').addEventListener('click', () => {
        modal.style.display = 'none';
    });
}

function hidePresetBrowser() {
    document.getElementById('preset-browser-modal').style.display = 'none';
    document.getElementById('preset-browser-btn').textContent = '|||';
}

function showPresetBrowser() {
    document.getElementById('preset-browser-modal').style.display = 'flex';
    document.getElementById('preset-browser-btn').textContent = '✕';
    // TODO: populate browser (call initPresetBrowser from preset-browser.js)
}

// Call on document ready
document.addEventListener('DOMContentLoaded', initPresetHeader);
