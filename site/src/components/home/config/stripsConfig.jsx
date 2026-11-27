import { useState, useEffect } from 'react';
import {
    Alert,
    Box,
    Button,
    Card,
    CardContent,
    Chip,
    Divider,
    FormControl,
    FormControlLabel,
    Icon,
    IconButton,
    InputLabel,
    MenuItem,
    Select,
    Switch,
    TextField,
    Typography
} from '@mui/material';
import PropTypes from 'prop-types';

const COLOR_ORDERS = ['GRB', 'RGB', 'RBG', 'GBR', 'BRG', 'BGR'];
const CHIPSETS = ['WS2812B', 'WS2811', 'WS2815', 'APA102', 'SK9822'];

const isSpiChipset = (chipset) => chipset === 'APA102' || chipset === 'SK9822';

/**
 * Visual canvas segmentation map
 */
const CanvasPreview = ({ strips }) => {
    const activeStrips = strips
        .map((s, origIdx) => ({ ...s, origIdx }))
        .filter(s => s.enabled);
    if (activeStrips.length === 0) return null;

    let maxPixel = 0;
    activeStrips.forEach(s => {
        const end = (s.offset || 0) + (s.length || 0);
        if (end > maxPixel) maxPixel = end;
    });

    if (maxPixel === 0) return null;

    const colors = ['#2196f3', '#4caf50', '#ff9800', '#e91e63', '#9c27b0', '#00bcd4'];

    return (
        <Box sx={{ mt: 2, mb: 2, p: 1.5, bgcolor: 'background.default', borderRadius: 1 }}>
            <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 1 }}>
                <Typography variant="caption" color="text.secondary">
                    Virtual Canvas Mapping (Total: {maxPixel} pixels)
                </Typography>
            </Box>
            <Box sx={{ display: 'flex', flexDirection: 'column', gap: 0.5 }}>
                {activeStrips.map((s) => {
                    const offset = s.offset || 0;
                    const length = s.length || 0;
                    const leftPercent = (offset / maxPixel) * 100;
                    const widthPercent = Math.max(2, (length / maxPixel) * 100);
                    return (
                        <Box
                            key={s.origIdx}
                            sx={{
                                position: 'relative',
                                width: '100%',
                                height: 22,
                                borderRadius: 1,
                                overflow: 'hidden',
                                bgcolor: 'action.hover'
                            }}
                        >
                            <Box
                                sx={{
                                    position: 'absolute',
                                    left: `${leftPercent}%`,
                                    width: `${Math.min(100 - leftPercent, widthPercent)}%`,
                                    height: '100%',
                                    bgcolor: colors[s.origIdx % colors.length],
                                    color: '#fff',
                                    display: 'flex',
                                    alignItems: 'center',
                                    justifyContent: 'center',
                                    fontSize: '0.72rem',
                                    fontWeight: 'bold',
                                    overflow: 'hidden',
                                    textOverflow: 'ellipsis',
                                    whiteSpace: 'nowrap',
                                    px: 0.5,
                                    borderRadius: 0.5
                                }}
                                title={`Ch ${s.origIdx}: Pin ${s.pin}, [${offset}..${offset + length}] (${length} px)`}
                            >
                                Ch {s.origIdx}: [{offset}..{offset + length}]
                            </Box>
                        </Box>
                    );
                })}
            </Box>
        </Box>
    );
};

CanvasPreview.propTypes = {
    strips: PropTypes.arrayOf(PropTypes.object).isRequired
};

/**
 * LED Strip Channels Editor Component
 */
const StripsConfigEditor = ({ strips, onChange, updateError }) => {
    const [localStrips, setLocalStrips] = useState(strips || []);

    // Detect duplicate pins across all active (enabled) strips
    const activeStrips = localStrips.filter(s => s.enabled);
    const pinCounts = {};
    activeStrips.forEach(s => {
        pinCounts[s.pin] = (pinCounts[s.pin] || 0) + 1;
    });
    const duplicatePins = new Set(
        Object.keys(pinCounts)
            .filter(p => pinCounts[p] > 1)
            .map(p => parseInt(p, 10))
    );

    useEffect(() => {
        if (updateError) {
            updateError('strips', duplicatePins.size > 0);
        }
    }, [duplicatePins.size, updateError]);

    const updateStrips = (newStrips) => {
        setLocalStrips(newStrips);
        if (onChange) {
            onChange(newStrips);
        }
    };

    const handleFieldChange = (index, field, value) => {
        const updated = localStrips.map((strip, i) => {
            if (i === index) {
                return { ...strip, [field]: value };
            }
            return strip;
        });
        updateStrips(updated);
    };

    const handleAddStrip = () => {
        let nextOffset = 0;
        let nextPin = 15;
        if (localStrips.length > 0) {
            const last = localStrips[localStrips.length - 1];
            nextOffset = (last.offset || 0) + (last.length || 0);

            // Select next unused pin starting from last.pin + 1 (up to 64 GPIOs for S31/P4)
            const usedPins = new Set(localStrips.map(s => s.pin));
            let candidatePin = (last.pin || 14) + 1;
            while (usedPins.has(candidatePin) && candidatePin < 64) {
                candidatePin++;
            }
            if (candidatePin < 64) {
                nextPin = candidatePin;
            } else {
                candidatePin = 1;
                while (usedPins.has(candidatePin) && candidatePin < 64) {
                    candidatePin++;
                }
                if (candidatePin < 64) nextPin = candidatePin;
            }
        }
        const newStrip = {
            pin: nextPin,
            clock: -1,
            length: 60,
            offset: nextOffset,
            order: 'GRB',
            chipset: 'WS2812B',
            enabled: true
        };
        updateStrips([...localStrips, newStrip]);
    };

    const handleRemoveStrip = (index) => {
        if (localStrips.length <= 1) return;
        const updated = localStrips.filter((_, i) => i !== index);
        updateStrips(updated);
    };

    return (
        <Box sx={{ mt: 3, mb: 2 }}>
            <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 1 }}>
                <Box sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
                    <Typography variant="h6" component="div">
                        LED Strip Channels
                    </Typography>
                    <Chip label={`${localStrips.length} Channel${localStrips.length !== 1 ? 's' : ''}`} size="small" color="primary" variant="outlined" />
                </Box>
                <Button
                    variant="contained"
                    size="small"
                    startIcon={<Icon>add</Icon>}
                    onClick={handleAddStrip}
                >
                    Add Channel
                </Button>
            </Box>
            <Typography variant="body2" color="text.secondary" sx={{ mb: 1.5 }}>
                Configure GPIO pins, pixel lengths, and canvas offsets for multi-strip daisy chains or mirrored outputs.
            </Typography>

            {duplicatePins.size > 0 && (
                <Alert severity="warning" sx={{ mb: 1.5 }}>
                    Duplicate GPIO pin detected ({Array.from(duplicatePins).map(p => `GPIO ${p}`).join(', ')}). Multiple active channels cannot drive the same pin.
                </Alert>
            )}

            <CanvasPreview strips={localStrips} />

            {localStrips.map((strip, idx) => {
                const isDup = strip.enabled && duplicatePins.has(strip.pin);
                return (
                    <Card key={idx} variant="outlined" sx={{ mb: 1.5, p: 0.5, bgcolor: strip.enabled ? 'background.paper' : 'action.disabledBackground', borderColor: isDup ? 'error.main' : undefined }}>
                        <CardContent sx={{ pb: '12px !important' }}>
                            <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 1 }}>
                                <Box sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
                                    <Chip label={`Channel ${idx}`} size="small" color="primary" />
                                    {isDup && (
                                        <Chip label="Duplicate Pin" size="small" color="error" variant="outlined" />
                                    )}
                                    <FormControlLabel
                                        control={
                                            <Switch
                                                size="small"
                                                checked={Boolean(strip.enabled)}
                                                onChange={(e) => handleFieldChange(idx, 'enabled', e.target.checked)}
                                            />
                                        }
                                        label={strip.enabled ? 'Enabled' : 'Disabled'}
                                    />
                                </Box>
                                <IconButton
                                    size="small"
                                    color="error"
                                    onClick={() => handleRemoveStrip(idx)}
                                    disabled={localStrips.length <= 1}
                                    title={localStrips.length <= 1 ? 'At least one channel is required' : `Delete Channel ${idx}`}
                                >
                                    <Icon>delete</Icon>
                                </IconButton>
                            </Box>

                            <Divider sx={{ mb: 1.5 }} />

                            <Box sx={{ display: 'flex', flexWrap: 'wrap', gap: 1.5 }}>
                                <TextField
                                    label="Data Pin (GPIO)"
                                    type="number"
                                    size="small"
                                    sx={{ minWidth: 120, flex: 1 }}
                                    value={strip.pin}
                                    onChange={(e) => handleFieldChange(idx, 'pin', parseInt(e.target.value, 10) || 0)}
                                    inputProps={{ min: 0, max: 64 }}
                                    error={isDup}
                                    helperText={isDup ? 'GPIO already in use' : ''}
                                />

                            {isSpiChipset(strip.chipset) && (
                                <TextField
                                    label="Clock Pin (GPIO)"
                                    type="number"
                                    size="small"
                                    sx={{ minWidth: 120, flex: 1 }}
                                    value={strip.clock >= 0 ? strip.clock : ''}
                                    onChange={(e) => handleFieldChange(idx, 'clock', e.target.value === '' ? -1 : parseInt(e.target.value, 10))}
                                    inputProps={{ min: 0, max: 64 }}
                                />
                            )}

                            <TextField
                                label="Length (LEDs)"
                                type="number"
                                size="small"
                                sx={{ minWidth: 120, flex: 1 }}
                                value={strip.length}
                                onChange={(e) => handleFieldChange(idx, 'length', Math.max(1, parseInt(e.target.value, 10) || 1))}
                                inputProps={{ min: 1, max: 4096 }}
                            />

                            <TextField
                                label="Canvas Offset"
                                type="number"
                                size="small"
                                sx={{ minWidth: 120, flex: 1 }}
                                value={strip.offset || 0}
                                onChange={(e) => handleFieldChange(idx, 'offset', Math.max(0, parseInt(e.target.value, 10) || 0))}
                                inputProps={{ min: 0, max: 4096 }}
                                helperText={strip.offset === 0 && idx > 0 ? 'Offset 0 = Mirroring Ch 0' : ''}
                            />

                            <FormControl size="small" sx={{ minWidth: 130, flex: 1 }}>
                                <InputLabel id={`order-label-${idx}`}>Color Order</InputLabel>
                                <Select
                                    labelId={`order-label-${idx}`}
                                    label="Color Order"
                                    value={strip.order || 'GRB'}
                                    onChange={(e) => handleFieldChange(idx, 'order', e.target.value)}
                                >
                                    {COLOR_ORDERS.map((ord) => (
                                        <MenuItem key={ord} value={ord}>
                                            {ord}
                                        </MenuItem>
                                    ))}
                                </Select>
                            </FormControl>

                            <FormControl size="small" sx={{ minWidth: 140, flex: 1 }}>
                                <InputLabel id={`chipset-label-${idx}`}>Chipset</InputLabel>
                                <Select
                                    labelId={`chipset-label-${idx}`}
                                    label="Chipset"
                                    value={strip.chipset || 'WS2812B'}
                                    onChange={(e) => handleFieldChange(idx, 'chipset', e.target.value)}
                                >
                                    {CHIPSETS.map((cs) => (
                                        <MenuItem key={cs} value={cs}>
                                            {cs}
                                        </MenuItem>
                                    ))}
                                </Select>
                            </FormControl>
                        </Box>
                    </CardContent>
                </Card>
            );
        })}
        </Box>
    );
};

StripsConfigEditor.propTypes = {
    strips: PropTypes.arrayOf(PropTypes.object),
    onChange: PropTypes.func,
    updateError: PropTypes.func
};

export default StripsConfigEditor;
