import gsd.hoomd

with gsd.hoomd.open('with-slashes.gsd', 'w') as gsdf_slash:
    with gsd.hoomd.open('without-slashes.gsd', 'w') as gsdf_noslash:
        for step in range(2):
            frame = gsd.hoomd.Frame()
            frame.particles.N = 1
            frame.particles.position = [[0, 0, 0]]
            key = 'name/space/property/value'
            frame.configuration.step = step
            frame.configuration.box = [1, 1, 1, 0, 0, 0]
            frame.log[key] = [0]
            gsdf_slash.append(frame)
            del frame.log[key]
            frame.log[key.replace('/', '.')] = [0]
            gsdf_noslash.append(frame)

for _filename in ['with-slashes.gsd', 'without-slashes.gsd']:
    print(_filename)
    with gsd.hoomd.open(_filename, 'r') as gsdf:
        for frame in gsdf:
            print(frame.log)
