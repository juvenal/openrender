import sys
import os

# Add src/python to path
sys.path.append(os.path.join(os.getcwd(), "src/python"))

import prman

def test_helloworld():
    ri = prman.Ri()
    rendertarget = "helloworld.rib"
    
    print(f"Generating {rendertarget}...")
    ri.Begin(rendertarget)
    
    # Matching sample image
    ri.Display('helloworld', 'framebuffer', 'rgb')
    ri.Format(640, 480, 1)
    ri.Projection(ri.PERSPECTIVE, {ri.FOV: 90})
    ri.Translate(0, 0, 10)
    
    ri.WorldBegin()
    ri.Color([1., 0, 0])
    ri.Translate(0, -1.5, 0)
    ri.Rotate(-110, 1, 0, 0)
    ri.Sphere(1, -1, 1, 360)
    ri.WorldEnd()
    
    ri.End()
    print("Done.")

    # Verify content
    if os.path.exists(rendertarget):
        with open(rendertarget, 'r') as f:
            content = f.read()
            print("--- Generated RIB ---")
            print(content)
            print("---------------------")
            
            # Simple assertions
            assert 'Display "helloworld" "framebuffer" "rgb"' in content
            assert 'Format 640 480 1' in content
            assert 'Projection "perspective" "fov" [90]' in content
            assert 'WorldBegin' in content
            assert 'Sphere 1 -1 1 360' in content
            assert 'WorldEnd' in content
            print("Verification successful.")
    else:
        print("Error: helloworld.rib not generated.")
        sys.exit(1)

def test_pipe():
    ri = prman.Ri()
    # Pipe to 'cat' and capture its output would be tricky here, 
    # but we can pipe to a file via shell redirection or just use 'cat > test_pipe.rib'
    # Actually, we can use a command that we know will run.
    
    print("Testing pipe to 'cat > test_pipe.rib'...")
    # Begin("cat", "> test_pipe.rib") might work if shell=True, but we use shlex.split.
    # Let's use a simpler command to just see if it runs without error.
    # On macOS/Linux 'cat' is always there.
    
    # We'll use a more explicit command that writes to a file to verify.
    ri.Begin("sh", "-c 'cat > test_pipe.rib'")
    ri.Display("pipe_test", "file", "rgba")
    ri.End()
    
    if os.path.exists("test_pipe.rib"):
        with open("test_pipe.rib", 'r') as f:
            content = f.read()
            print("--- Pipe Output ---")
            print(content)
            print("-------------------")
            assert 'Display "pipe_test" "file" "rgba"' in content
            print("Pipe test successful.")
    else:
        print("Error: test_pipe.rib not generated via pipe.")
        sys.exit(1)

def test_comprehensive():
    ri = prman.Ri()
    rendertarget = "comprehensive.rib"
    ri.Begin(rendertarget)
    
    ri.Option("limits", {"bucketsize": [16, 16], "gridsize": 32})
    ri.Display("test", "file", "rgba")
    ri.Format(1024, 768, 1)
    
    ri.TransformBegin()
    ri.Translate(1, 2, 3)
    ri.Rotate(45, 0, 1, 0)
    
    # Object definition
    handle = ri.ObjectBegin()
    ri.Sphere(1, -1, 1, 360)
    ri.ObjectEnd()
    
    ri.WorldBegin()
    
    # Motion block
    ri.MotionBegin([0.0, 1.0])
    ri.Translate(0, 0, 0)
    ri.Translate(0, 1, 0)
    ri.MotionEnd()
    
    # Solid block
    ri.SolidBegin(ri.UNION)
    ri.Sphere(1, -1, 1, 360)
    ri.Cone(2, 1, 360)
    ri.SolidEnd()
    
    ri.ObjectInstance(handle)
    
    ri.Geometry("teapot")
    
    ri.WorldEnd()
    ri.TransformEnd()
    ri.End()

    if os.path.exists(rendertarget):
        with open(rendertarget, 'r') as f:
            content = f.read()
            print("--- Comprehensive RIB ---")
            print(content)
            print("-------------------------")
            
            assert 'TransformBegin' in content
            assert '  Translate 1 2 3' in content # Check indentation
            assert 'ObjectBegin 1' in content
            assert 'ObjectEnd' in content
            assert 'MotionBegin [0.0 1.0]' in content
            assert 'SolidBegin "union"' in content
            assert 'ObjectInstance 1' in content
            assert 'Geometry "teapot"' in content
            print("Comprehensive test successful.")

if __name__ == "__main__":
    test_helloworld()
    test_pipe()
    test_comprehensive()
