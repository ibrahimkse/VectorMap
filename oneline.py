import osmium
import xml.etree.ElementTree as ET
import xml.dom.minidom

class FirstPassHandler(osmium.SimpleHandler):
    def __init__(self):
        super(FirstPassHandler, self).__init__()
        self.turkey_relation_id = None
        self.boundary_ways = set()

    def relation(self, r):
        if 'name:tr' in r.tags and r.tags['name:tr'] == 'Rusya Federasyonu':
            self.turkey_relation_id = r.id
            for member in r.members:
                if member.type == 'w' and member.role == 'outer':
                    self.boundary_ways.add(member.ref)

class SecondPassHandler(osmium.SimpleHandler):
    def __init__(self, boundary_ways):
        super(SecondPassHandler, self).__init__()
        self.boundary_ways = boundary_ways
        self.ways = {}

    def way(self, w):
        if w.id in self.boundary_ways:
            self.ways[w.id] = [(node.ref, node.location.lat, node.location.lon) for node in w.nodes]

    def node(self, n):
        pass  # We no longer need to handle nodes here

# First pass to get the Turkey relation and boundary ways
first_pass_handler = FirstPassHandler()
first_pass_handler.apply_file("C:/Users/DMAP/Downloads/russia-latest.osm.pbf", locations=True)

# Second pass to get the nodes for the boundary ways
second_pass_handler = SecondPassHandler(first_pass_handler.boundary_ways)
second_pass_handler.apply_file("C:/Users/DMAP/Downloads/russia-latest.osm.pbf", locations=True)

# Create XML structure
root = ET.Element("osm")
for way_id, nodes in second_pass_handler.ways.items():
    way_elem = ET.SubElement(root, "way", id=str(way_id))
    for node_id, lat, lon in nodes:
        node_elem = ET.SubElement(way_elem, "node")
        node_elem.set("id", str(node_id))
        node_elem.set("lat", str(lat))
        node_elem.set("lon", str(lon))

# Convert the ElementTree to a string
xml_str = ET.tostring(root, encoding="utf-8", method="xml")

# Parse the string using minidom for pretty printing
xml_str_pretty = xml.dom.minidom.parseString(xml_str).toprettyxml(indent="  ")

# Write the pretty-printed XML to a file
with open("russia_border1.xml", "w", encoding="utf-8") as f:
    f.write(xml_str_pretty)

print("XML has been saved")
