// Simple preset dropdown (organized by category)

function initPresetDropdown() {
    const presetNameDisplay = document.getElementById('preset-name');

    presetNameDisplay.addEventListener('click', (e) => {
        e.stopPropagation();
        toggleDropdown();
    });

    // Close dropdown on document click
    document.addEventListener('click', () => {
        const dropdown = document.getElementById('preset-dropdown');
        if (dropdown.style.display !== 'none') {
            dropdown.style.display = 'none';
        }
    });
}

function toggleDropdown() {
    const dropdown = document.getElementById('preset-dropdown');
    if (dropdown.style.display === 'none') {
        populateDropdown();
        dropdown.style.display = 'block';
    } else {
        dropdown.style.display = 'none';
    }
}

function populateDropdown() {
    const dropdown = document.getElementById('preset-dropdown');

    // Get all presets from native function
    if (!window.Juce || !window.Juce.getAllPresets) {
        dropdown.innerHTML = '<div style="padding: 8px; color: #999;">No presets available</div>';
        return;
    }

    const presetsJson = window.Juce.getAllPresets();
    const presets = JSON.parse(presetsJson);

    let html = '<div style="padding: 8px; border-bottom: 1px solid #16384d; color: #999; font-size: 9px; text-transform: uppercase;">Categories</div>';

    // Group by type/category
    const categories = {};
    for (const [packName, presetList] of Object.entries(presets)) {
        for (const preset of presetList) {
            const type = preset.metadata.type || 'Uncategorized';
            if (!categories[type]) {
                categories[type] = [];
            }
            categories[type].push({ name: preset.metadata.name, packName: packName });
        }
    }

    // Render categories
    for (const [type, typePresets] of Object.entries(categories)) {
        html += `<div style="padding: 6px 8px; background: #16384d; cursor: pointer; color: #16c784; user-select: none;" onclick="toggleCategory('${type}')">▼ ${type}</div>`;
        html += `<div id="category-${type}" style="display: block;">`;

        for (const p of typePresets) {
            const isCurrent = p.name === currentPresetName;
            const bgColor = isCurrent ? '#16384d' : '#0f3460';
            const textColor = isCurrent ? '#16c784' : '#999';
            html += `<div style="padding: 6px 12px; cursor: pointer; background: ${bgColor}; color: ${textColor}; border-left: 2px solid ${isCurrent ? '#16c784' : 'transparent'};" onclick="loadPresetFromDropdown('${p.name}', '${p.packName}')">
                ${isCurrent ? '▶ ' : ''}${p.name}
            </div>`;
        }
        html += '</div>';
    }

    dropdown.innerHTML = html;
}

function toggleCategory(type) {
    const categoryDiv = document.getElementById(`category-${type}`);
    if (categoryDiv) {
        categoryDiv.style.display = categoryDiv.style.display === 'none' ? 'block' : 'none';
    }
}

function loadPresetFromDropdown(presetName, packName) {
    // Call native function to load preset
    if (window.Juce && window.Juce.loadPreset) {
        window.Juce.loadPreset(presetName, packName);
        setCurrentPreset(presetName, false, packName);
        document.getElementById('preset-dropdown').style.display = 'none';
    }
}
