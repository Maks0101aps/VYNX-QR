import { useCallback, useRef, useState } from 'react';

export interface Toast {
  id: number;
  message: string;
  tone: 'neutral' | 'success' | 'danger';
}

interface UseToastsResult {
  toasts: Toast[];
  push: (message: string, tone?: Toast['tone']) => void;
  dismiss: (id: number) => void;
}

const LIFETIME = 2800;
const MAX_VISIBLE = 3;

/** Small in-app notification stack. No Windows toasts, no popups. */
export function useToasts(): UseToastsResult {
  const [toasts, setToasts] = useState<Toast[]>([]);
  const nextId = useRef(1);

  const dismiss = useCallback((id: number) => {
    setToasts((current) => current.filter((toast) => toast.id !== id));
  }, []);

  const push = useCallback(
    (message: string, tone: Toast['tone'] = 'neutral') => {
      const id = nextId.current;
      nextId.current += 1;
      setToasts((current) => [...current, { id, message, tone }].slice(-MAX_VISIBLE));
      window.setTimeout(() => dismiss(id), LIFETIME);
    },
    [dismiss],
  );

  return { toasts, push, dismiss };
}
