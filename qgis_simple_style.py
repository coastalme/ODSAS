#!/usr/bin/env python3
"""
Simple QGIS Script: Bidirectional ODSAS Normals with Center Scaling
==================================================================

Bidirectional scaling script for ODSAS coastal change normals.
FEATURES: 
- Lines scale FROM CENTER of original transects
- Positive values → Green lines toward user-selected direction (start/end)
- Negative values → Red lines toward opposite direction
- Near-zero values → Short white centered lines
- Interactive direction selection

Usage in QGIS Python Console:
1. Load your normals GPKG layer in QGIS and select it
2. Run this script
3. Choose field name and direction for positive values

Author: Andres Payo
"""

from qgis.core import (
    QgsVectorLayer, QgsLineSymbol, QgsGraduatedSymbolRenderer, 
    QgsRendererRange, QgsProject, QgsGeometry, QgsFeature, QgsField,
    QgsWkbTypes
)
from qgis.PyQt.QtCore import QVariant
from qgis.PyQt.QtGui import QColor
from qgis.PyQt.QtWidgets import QInputDialog, QMessageBox

def create_scaled_layer(layer, field_name, positive_direction='end'):
    """
    Create a memory layer with scaled line geometries.
    """
    # Get field values to determine scaling
    values = []
    for feature in layer.getFeatures():
        val = feature[field_name]
        if val is not None:
            try:
                values.append(float(val))
            except:
                continue
    
    if not values:
        return None
    
    max_abs_val = max(abs(min(values)), abs(max(values)))
    
    # Create memory layer
    crs = layer.crs().authid()
    scaled_layer = QgsVectorLayer(f"LineString?crs={crs}", f"Scaled {layer.name()}", "memory")
    
    # Copy fields
    scaled_layer.dataProvider().addAttributes(layer.fields())
    scaled_layer.dataProvider().addAttributes([QgsField("scale_factor", QVariant.Double)])
    scaled_layer.updateFields()
    
    scaled_features = []
    
    for feature in layer.getFeatures():
        val = feature[field_name]
        if val is None:
            continue
            
        try:
            val = float(val)
        except:
            continue
        
        # Calculate scale factor (0.3 to 2.0)
        if max_abs_val > 0:
            norm_val = abs(val) / max_abs_val
            scale_factor = 0.3 + (norm_val * 1.7)
        else:
            scale_factor = 1.0
        
        # Scale geometry from center with direction
        geom = feature.geometry()
        if geom and not geom.isEmpty():
            scaled_geom = scale_bidirectional_line(geom, val, max_abs_val, positive_direction)
            if scaled_geom:
                new_feature = QgsFeature(scaled_layer.fields())
                new_feature.setGeometry(scaled_geom)
                
                # Copy attributes
                for field in layer.fields():
                    new_feature[field.name()] = feature[field.name()]
                new_feature["scale_factor"] = scale_factor
                
                scaled_features.append(new_feature)
    
    # Add features
    scaled_layer.dataProvider().addFeatures(scaled_features)
    scaled_layer.updateExtents()
    
    return scaled_layer

def scale_bidirectional_line(geometry, value, max_abs_val, positive_direction='end'):
    """
    Scale line from center point with bidirectional scaling based on value sign.
    Positive values go toward start or end, negative go opposite direction.
    """
    if geometry.wkbType() != QgsWkbTypes.LineString:
        return None
        
    line = geometry.asPolyline()
    if len(line) < 2:
        return None
    
    # Calculate center point
    start_pt = line[0]
    end_pt = line[-1]
    center_x = (start_pt.x() + end_pt.x()) / 2.0
    center_y = (start_pt.y() + end_pt.y()) / 2.0
    center_pt = type(start_pt)(center_x, center_y)
    
    # Calculate scale factor (0.1 to 1.0 based on absolute value)
    if max_abs_val > 0:
        norm_val = abs(value) / max_abs_val
        scale_factor = 0.1 + (norm_val * 0.9)  # Scale from 0.1 to 1.0
    else:
        scale_factor = 0.1
    
    # Determine direction based on value sign and user preference
    if value >= 0:
        # Positive values go toward chosen direction
        if positive_direction == 'end':
            target_pt = end_pt
        else:
            target_pt = start_pt
    else:
        # Negative values go opposite direction
        if positive_direction == 'end':
            target_pt = start_pt
        else:
            target_pt = end_pt
    
    # Calculate scaled endpoint
    dx = target_pt.x() - center_x
    dy = target_pt.y() - center_y
    
    scaled_x = center_x + (dx * scale_factor)
    scaled_y = center_y + (dy * scale_factor)
    scaled_end = type(center_pt)(scaled_x, scaled_y)
    
    # Create line from center to scaled endpoint
    return QgsGeometry.fromPolylineXY([center_pt, scaled_end])

def quick_style_normals_with_length():
    """
    Quick styling for ODSAS normals with red-white-green colors and length scaling.
    """
    # Get active layer
    try:
        layer = iface.activeLayer()
    except NameError:
        print("❌ This script must be run in QGIS Python Console")
        return
        
    if not layer or not isinstance(layer, QgsVectorLayer):
        print("❌ Please select a vector layer first")
        return
    
    # Get field names
    field_names = [field.name() for field in layer.fields()]
    coastal_fields = []
    for field in field_names:
        if any(metric in field.upper() for metric in ['NSM', 'EPR', 'LRR', 'WLR', 'SCE']):
            coastal_fields.append(field)
    
    if not coastal_fields:
        print("🔍 Available fields:", field_names)
        field_name, ok = QInputDialog.getItem(None, "Select Field", "Choose field:", field_names, 0, False)
    else:
        field_name, ok = QInputDialog.getItem(None, "Select Coastal Metric", "Choose field:", coastal_fields, 0, False)
    
    if not ok:
        return
    
    # Ask user for positive direction
    direction_options = ['end', 'start']
    direction, ok = QInputDialog.getItem(None, "Positive Direction", 
                                        "Direction for positive values:\n" +
                                        "'end' = toward end of transect\n" +
                                        "'start' = toward start of transect", 
                                        direction_options, 0, False)
    if not ok:
        return
    
    print(f"📏 Creating bidirectional scaled layer for field: {field_name}")
    print(f"   ➡️ Positive direction: toward {direction} of transect")
    
    # Create scaled layer
    scaled_layer = create_scaled_layer(layer, field_name, direction)
    if not scaled_layer:
        print("❌ Failed to create scaled layer")
        return
    
    # Add to project
    QgsProject.instance().addMapLayer(scaled_layer)
    
    # Get values for color ranges
    values = []
    for feature in scaled_layer.getFeatures():
        val = feature[field_name]
        if val is not None:
            try:
                values.append(float(val))
            except:
                continue
    
    if not values:
        print("❌ No valid values found")
        return
    
    min_val = min(values)
    max_val = max(values)
    
    print(f"   Value range: {min_val:.3f} to {max_val:.3f}")
    
    # Create bidirectional color-coded ranges
    ranges = []
    
    if min_val >= 0:
        # All positive - light to dark green
        mid = min_val + (max_val - min_val) * 0.5
        ranges = [
            (min_val, mid, QColor(144, 238, 144), f"Low Accretion ({min_val:.2f} to {mid:.2f})"),
            (mid, max_val, QColor(34, 139, 34), f"High Accretion ({mid:.2f} to {max_val:.2f})")
        ]
    elif max_val <= 0:
        # All negative - light to dark red
        mid = min_val + (max_val - min_val) * 0.5
        ranges = [
            (min_val, mid, QColor(220, 20, 60), f"High Erosion ({min_val:.2f} to {mid:.2f})"),
            (mid, max_val, QColor(255, 182, 193), f"Low Erosion ({mid:.2f} to {max_val:.2f})")
        ]
    else:
        # Bidirectional - red for negative, white for near-zero, green for positive
        abs_max = max(abs(min_val), abs(max_val))
        threshold = abs_max * 0.05  # 5% threshold for "stable"
        
        ranges = [
            (min_val, -threshold, QColor(220, 20, 60), f"Erosion ({min_val:.2f} to {-threshold:.2f})"),
            (-threshold, threshold, QColor(245, 245, 245), f"Stable ({-threshold:.2f} to {threshold:.2f})"),
            (threshold, max_val, QColor(34, 139, 34), f"Accretion ({threshold:.2f} to {max_val:.2f})")
        ]
    
    # Create graduated ranges
    graduated_ranges = []
    for lower, upper, color, label in ranges:
        symbol = QgsLineSymbol.createSimple({
            'color': color.name(),
            'width': '1.2',
            'capstyle': 'round'
        })
        
        range_obj = QgsRendererRange(lower, upper, symbol, label)
        graduated_ranges.append(range_obj)
    
    # Apply renderer
    renderer = QgsGraduatedSymbolRenderer(field_name, graduated_ranges)
    scaled_layer.setRenderer(renderer)
    scaled_layer.triggerRepaint()
    
    # Hide original layer
    legend_original = QgsProject.instance().layerTreeRoot().findLayer(layer.id())
    if legend_original:
        legend_original.setItemVisibilityChecked(False)
    
    # Refresh legend
    try:
        iface.layerTreeView().refreshLayerSymbology(scaled_layer.id())
    except:
        pass
    
    print(f"✅ Bidirectional scaling applied!")
    print(f"   📏 Lines scaled from center point")
    print(f"   🔴 Red = Erosion (toward {'start' if direction == 'end' else 'end'})")
    print(f"   ⚪ White = Stable (short centered lines)")
    print(f"   🟢 Green = Accretion (toward {direction})")
    
    return scaled_layer

# Run the function
if __name__ == "__main__":
    quick_style_normals_with_length()

print("""
ODSAS Bidirectional Normals Styling Script Loaded!

NEW FEATURE: 📏 BIDIRECTIONAL CENTER SCALING
• Lines scale from CENTER of original transect
• Positive values = Green lines toward chosen direction
• Negative values = Red lines toward opposite direction
• Near-zero values = Short white lines at center
• User selects direction for positive values

Run: quick_style_normals_with_length()

Colors: 🔴 Red = Erosion | ⚪ White = Stable | 🟢 Green = Accretion
""")