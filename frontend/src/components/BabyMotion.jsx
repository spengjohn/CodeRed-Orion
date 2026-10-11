import { useEffect, useRef } from 'react'
import * as THREE from 'three'

// Small 3D crib for the Activity card: the baby rocks and shuffles around
// while moving, and settles back to lie still (breathing gently) otherwise.
function BabyMotion({ isMoving }) {
  const mount = useRef(null)
  const moving = useRef(isMoving)

  useEffect(() => {
    moving.current = isMoving
  }, [isMoving])

  useEffect(() => {
    const el = mount.current
    let renderer
    try {
      renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true })
    } catch {
      return undefined // no WebGL: the card just shows its text
    }
    renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2))
    el.appendChild(renderer.domElement)

    const scene = new THREE.Scene()
    const camera = new THREE.PerspectiveCamera(30, 1, 0.1, 50)
    camera.position.set(0, 2, 2.3)
    camera.lookAt(0, 0.15, 0)
    scene.add(new THREE.AmbientLight(0xffffff, 1.6))
    const sun = new THREE.DirectionalLight(0xffffff, 1.8)
    sun.position.set(2, 4, 3)
    scene.add(sun)

    // Mattress
    const mattress = new THREE.Mesh(
      new THREE.BoxGeometry(3, 0.2, 1.6),
      new THREE.MeshStandardMaterial({ color: '#f2f0ea' }),
    )
    mattress.position.y = -0.1
    scene.add(mattress)

    // Baby: a swaddle in the card's accent color and a head
    const accent = getComputedStyle(el).getPropertyValue('--m').trim() || '#9483bf'
    const body = new THREE.Mesh(
      new THREE.CapsuleGeometry(0.28, 0.6, 8, 16),
      new THREE.MeshStandardMaterial({ color: accent }),
    )
    body.rotation.z = Math.PI / 2 // lying along the mattress
    body.position.set(0.25, 0.28, 0)
    const head = new THREE.Mesh(
      new THREE.SphereGeometry(0.3, 24, 16),
      new THREE.MeshStandardMaterial({ color: '#f2c9ab' }),
    )
    head.position.set(-0.55, 0.3, 0)
    const baby = new THREE.Group()
    baby.add(body, head)
    scene.add(baby)

    // Keep the canvas sized to the card.
    const resize = () => {
      const { clientWidth: w, clientHeight: h } = el
      if (!w || !h) return
      renderer.setSize(w, h)
      camera.aspect = w / h
      camera.updateProjectionMatrix()
    }
    const observer = new ResizeObserver(resize)
    observer.observe(el)
    resize()

    const reduceMotion = window.matchMedia?.('(prefers-reduced-motion: reduce)').matches
    const clock = new THREE.Clock()
    let frame
    const animate = () => {
      const t = clock.getElapsedTime()
      if (moving.current && !reduceMotion) {
        baby.rotation.x = Math.sin(t * 3) * 0.35 // rocking side to side
        baby.position.x = Math.sin(t * 0.8) * 0.4 // shuffling along the mattress
        baby.position.z = Math.sin(t * 1.3) * 0.15
      } else {
        // Ease back to lying still in the middle.
        baby.rotation.x *= 0.9
        baby.position.x *= 0.92
        baby.position.z *= 0.92
      }
      body.scale.x = 1 + Math.sin(t * 2.4) * 0.04 // gentle breathing
      renderer.render(scene, camera)
      frame = requestAnimationFrame(animate)
    }
    animate()

    return () => {
      cancelAnimationFrame(frame)
      observer.disconnect()
      scene.traverse((obj) => {
        obj.geometry?.dispose()
        obj.material?.dispose()
      })
      renderer.dispose()
      el.removeChild(renderer.domElement)
    }
  }, [])

  return <span className="m-visual" ref={mount} aria-hidden="true" />
}

export default BabyMotion
